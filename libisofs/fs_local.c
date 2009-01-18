/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

/*
 * Filesystem/FileSource implementation to access the local filesystem.
 */

/* ts A90116 : libisofs.h eventually defines Libisofs_with_aaiP */
#include "libisofs.h"

#include "fsource.h"
#include "util.h"

#ifdef Libisofs_with_aaiP
#include "aaip_0_2.h"
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>

static
int iso_file_source_new_lfs(IsoFileSource *parent, const char *name, 
                            IsoFileSource **src);

/*
 * We can share a local filesystem object, as it has no private atts.
 */
IsoFilesystem *lfs= NULL;

typedef struct
{
    /** reference to the parent (if root it points to itself) */
    IsoFileSource *parent;
    char *name;
    unsigned int openned :2; /* 0: not openned, 1: file, 2:dir */
    union
    {
        int fd;
        DIR *dir;
    } info;
} _LocalFsFileSource;

static
char* lfs_get_path(IsoFileSource *src)
{
    _LocalFsFileSource *data;
    data = src->data;

    if (data->parent == src) {
        return strdup("/");
    } else {
        char *path = lfs_get_path(data->parent);
        int pathlen = strlen(path);
        path = realloc(path, pathlen + strlen(data->name) + 2);
        if (pathlen != 1) {
            /* pathlen can only be 1 for root */
            path[pathlen] = '/';
            path[pathlen + 1] = '\0';
        }
        return strcat(path, data->name);
    }
}

static
char* lfs_get_name(IsoFileSource *src)
{
    _LocalFsFileSource *data;
    data = src->data;
    return strdup(data->name);
}

static
int lfs_lstat(IsoFileSource *src, struct stat *info)
{
    _LocalFsFileSource *data;
    char *path;

    if (src == NULL || info == NULL) {
        return ISO_NULL_POINTER;
    }
    data = src->data;
    path = lfs_get_path(src);

    if (lstat(path, info) != 0) {
        int err;

        /* error, choose an appropriate return code */
        switch (errno) {
        case EACCES:
            err = ISO_FILE_ACCESS_DENIED;
            break;
        case ENOTDIR:
        case ENAMETOOLONG:
        case ELOOP:
            err = ISO_FILE_BAD_PATH;
            break;
        case ENOENT:
            err = ISO_FILE_DOESNT_EXIST;
            break;
        case EFAULT:
        case ENOMEM:
            err = ISO_OUT_OF_MEM;
            break;
        default:
            err = ISO_FILE_ERROR;
            break;
        }
        return err;
    }
    free(path);
    return ISO_SUCCESS;
}

static
int lfs_stat(IsoFileSource *src, struct stat *info)
{
    _LocalFsFileSource *data;
    char *path;

    if (src == NULL || info == NULL) {
        return ISO_NULL_POINTER;
    }
    data = src->data;
    path = lfs_get_path(src);

    if (stat(path, info) != 0) {
        int err;

        /* error, choose an appropriate return code */
        switch (errno) {
        case EACCES:
            err = ISO_FILE_ACCESS_DENIED;
            break;
        case ENOTDIR:
        case ENAMETOOLONG:
        case ELOOP:
            err = ISO_FILE_BAD_PATH;
            break;
        case ENOENT:
            err = ISO_FILE_DOESNT_EXIST;
            break;
        case EFAULT:
        case ENOMEM:
            err = ISO_OUT_OF_MEM;
            break;
        default:
            err = ISO_FILE_ERROR;
            break;
        }
        return err;
    }
    free(path);
    return ISO_SUCCESS;
}

static
int lfs_access(IsoFileSource *src)
{
    int ret;
    _LocalFsFileSource *data;
    char *path;

    if (src == NULL) {
        return ISO_NULL_POINTER;
    }
    data = src->data;
    path = lfs_get_path(src);

    ret = iso_eaccess(path);
    free(path);
    return ret;
}

static
int lfs_open(IsoFileSource *src)
{
    int err;
    struct stat info;
    _LocalFsFileSource *data;
    char *path;

    if (src == NULL) {
        return ISO_NULL_POINTER;
    }

    data = src->data;
    if (data->openned) {
        return ISO_FILE_ALREADY_OPENED;
    }

    /* is a file or a dir ? */
    err = lfs_stat(src, &info);
    if (err < 0) {
        return err;
    }

    path = lfs_get_path(src);
    if (S_ISDIR(info.st_mode)) {
        data->info.dir = opendir(path);
        data->openned = data->info.dir ? 2 : 0;
    } else {
        data->info.fd = open(path, O_RDONLY);
        data->openned = data->info.fd != -1 ? 1 : 0;
    }
    free(path);

    /*
     * check for possible errors, note that many of possible ones are
     * parsed in the lstat call above 
     */
    if (data->openned == 0) {
        switch (errno) {
        case EACCES:
            err = ISO_FILE_ACCESS_DENIED;
            break;
        case EFAULT:
        case ENOMEM:
            err = ISO_OUT_OF_MEM;
            break;
        default:
            err = ISO_FILE_ERROR;
            break;
        }
        return err;
    }

    return ISO_SUCCESS;
}

static
int lfs_close(IsoFileSource *src)
{
    int ret;
    _LocalFsFileSource *data;

    if (src == NULL) {
        return ISO_NULL_POINTER;
    }

    data = src->data;
    switch (data->openned) {
    case 1: /* not dir */
        ret = close(data->info.fd) == 0 ? ISO_SUCCESS : ISO_FILE_ERROR;
        break;
    case 2: /* directory */
        ret = closedir(data->info.dir) == 0 ? ISO_SUCCESS : ISO_FILE_ERROR;
        break;
    default:
        ret = ISO_FILE_NOT_OPENED;
        break;
    }
    if (ret == ISO_SUCCESS) {
        data->openned = 0;
    }
    return ret;
}

static
int lfs_read(IsoFileSource *src, void *buf, size_t count)
{
    _LocalFsFileSource *data;

    if (src == NULL || buf == NULL) {
        return ISO_NULL_POINTER;
    }
    if (count == 0) {
        return ISO_WRONG_ARG_VALUE;
    }

    data = src->data;
    switch (data->openned) {
    case 1: /* not dir */
        {
            int ret;
            ret = read(data->info.fd, buf, count);
            if (ret < 0) {
                /* error on read */
                switch (errno) {
                case EINTR:
                    ret = ISO_INTERRUPTED;
                    break;
                case EFAULT:
                    ret = ISO_OUT_OF_MEM;
                    break;
                case EIO:
                    ret = ISO_FILE_READ_ERROR;
                    break;
                default:
                    ret = ISO_FILE_ERROR;
                    break;
                }
            }
            return ret;
        }
    case 2: /* directory */
        return ISO_FILE_IS_DIR;
    default:
        return ISO_FILE_NOT_OPENED;
    }
}

static
off_t lfs_lseek(IsoFileSource *src, off_t offset, int flag)
{
    _LocalFsFileSource *data;
    int whence;

    if (src == NULL) {
        return (off_t)ISO_NULL_POINTER;
    }
    switch (flag) {
    case 0: 
        whence = SEEK_SET; break;
    case 1: 
        whence = SEEK_CUR; break;
    case 2: 
        whence = SEEK_END; break;
    default: 
        return (off_t)ISO_WRONG_ARG_VALUE;
    }

    data = src->data;
    switch (data->openned) {
    case 1: /* not dir */
        {
            off_t ret;
            ret = lseek(data->info.fd, offset, whence);
            if (ret < 0) {
                /* error on read */
                switch (errno) {
                case ESPIPE:
                    ret = (off_t)ISO_FILE_ERROR;
                    break;
                default:
                    ret = (off_t)ISO_ERROR;
                    break;
                }
            }
            return ret;
        }
    case 2: /* directory */
        return (off_t)ISO_FILE_IS_DIR;
    default:
        return (off_t)ISO_FILE_NOT_OPENED;
    }
}

static
int lfs_readdir(IsoFileSource *src, IsoFileSource **child)
{
    _LocalFsFileSource *data;

    if (src == NULL || child == NULL) {
        return ISO_NULL_POINTER;
    }

    data = src->data;
    switch (data->openned) {
    case 1: /* not dir */
        return ISO_FILE_IS_NOT_DIR;
    case 2: /* directory */
        {
            struct dirent *entry;
            int ret;

            /* while to skip "." and ".." dirs */
            while (1) {
                entry = readdir(data->info.dir);
                if (entry == NULL) {
                    if (errno == EBADF)
                        return ISO_FILE_ERROR;
                    else
                        return 0; /* EOF */
                }
                if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                    break;
                }
            }

            /* create the new FileSrc */
            ret = iso_file_source_new_lfs(src, entry->d_name, child);
            return ret;
        }
    default:
        return ISO_FILE_NOT_OPENED;
    }
}

static
int lfs_readlink(IsoFileSource *src, char *buf, size_t bufsiz)
{
    int size;
    _LocalFsFileSource *data;
    char *path;

    if (src == NULL || buf == NULL) {
        return ISO_NULL_POINTER;
    }

    if (bufsiz <= 0) {
        return ISO_WRONG_ARG_VALUE;
    }

    data = src->data;
    path = lfs_get_path(src);

    /*
     * invoke readlink, with bufsiz -1 to reserve an space for
     * the NULL character
     */
    size = readlink(path, buf, bufsiz - 1);
    free(path);
    if (size < 0) {
        /* error */
        switch (errno) {
        case EACCES:
            return ISO_FILE_ACCESS_DENIED;
        case ENOTDIR:
        case ENAMETOOLONG:
        case ELOOP:
            return ISO_FILE_BAD_PATH;
        case ENOENT:
            return ISO_FILE_DOESNT_EXIST;
        case EINVAL:
            return ISO_FILE_IS_NOT_SYMLINK;
        case EFAULT:
        case ENOMEM:
            return ISO_OUT_OF_MEM;
        default:
            return ISO_FILE_ERROR;
        }
    }

    /* NULL-terminate the buf */
    buf[size] = '\0';
    return ISO_SUCCESS;
}

static
IsoFilesystem* lfs_get_filesystem(IsoFileSource *src)
{
    return src == NULL ? NULL : lfs;
}

static
void lfs_free(IsoFileSource *src)
{
    _LocalFsFileSource *data;

    data = src->data;

    /* close the file if it is already openned */
    if (data->openned) {
        src->class->close(src);
    }
    if (data->parent != src) {
        iso_file_source_unref(data->parent);
    }
    free(data->name);
    free(data);
    iso_filesystem_unref(lfs);
}


    
#ifdef Libisofs_with_aaiP
/* ts A90116 */

static 
int lfs_get_aa_string(IsoFileSource *src, unsigned char **aa_string, int flag)
{
    unsigned int uret;
    int ret;
    size_t num_attrs = 0, *value_lengths = NULL, result_len;
    char *path = NULL, **names = NULL, **values = NULL;
    unsigned char *result = NULL;

    *aa_string = NULL;
    /* Obtain EAs and ACLs ("access" and "default"). ACLs encoded according
       to AAIP ACL representation. Clean out st_mode ACL entries.
    */ 
    path = iso_file_source_get_path(src);

    /* >>> make adjustable: bit4 = ignoring of st_mode ACL entries */
    ret = aaip_get_attr_list(path, &num_attrs, &names,
                             &value_lengths, &values, 1 | 2 | 16);
    if (ret <= 0) {
        ret = ISO_FILE_ERROR;
        goto ex;
    }
    uret = aaip_encode("AA", (unsigned int) num_attrs, names,
                       value_lengths, values, &result_len, &result, 0);
    if (uret == 0) {
        ret = ISO_OUT_OF_MEM;
        goto ex;
    }
    *aa_string = result;
    ret = 1;
ex:;
    if (path != NULL)
        free(path);
    if (names != NULL || value_lengths != NULL || values != NULL)
        aaip_get_attr_list(path, &num_attrs, &names, &value_lengths, &values,
                           1 << 15); /* free memory */
    return ret;
}

#endif /* Libisofs_with_aaiP */


IsoFileSourceIface lfs_class = { 

#ifdef Libisofs_with_aaiP
    1, /* version */
#else
    0, /* version */
#endif

    lfs_get_path,
    lfs_get_name,
    lfs_lstat,
    lfs_stat,
    lfs_access,
    lfs_open,
    lfs_close,
    lfs_read,
    lfs_readdir,
    lfs_readlink,
    lfs_get_filesystem,
    lfs_free,
    lfs_lseek

#ifdef Libisofs_with_aaiP
    ,
    lfs_get_aa_string
#endif

};


/**
 * 
 * @return
 *     1 success, < 0 error
 */
static
int iso_file_source_new_lfs(IsoFileSource *parent, const char *name, 
                            IsoFileSource **src)
{
    IsoFileSource *lfs_src;
    _LocalFsFileSource *data;

    if (src == NULL) {
        return ISO_NULL_POINTER;
    }

    if (lfs == NULL) {
        /* this should never happen */
        return ISO_ASSERT_FAILURE;
    }

    /* allocate memory */
    data = malloc(sizeof(_LocalFsFileSource));
    if (data == NULL) {
        return ISO_OUT_OF_MEM;
    }
    lfs_src = malloc(sizeof(IsoFileSource));
    if (lfs_src == NULL) {
        free(data);
        return ISO_OUT_OF_MEM;
    }

    /* fill struct */
    data->name = name ? strdup(name) : NULL;
    data->openned = 0;
    if (parent) {
        data->parent = parent;
        iso_file_source_ref(parent);
    } else {
        data->parent = lfs_src;
    }

    lfs_src->refcount = 1;
    lfs_src->data = data;
    lfs_src->class = &lfs_class;

    /* take a ref to local filesystem */
    iso_filesystem_ref(lfs);

    /* return */
    *src = lfs_src;
    return ISO_SUCCESS;
}

static
int lfs_get_root(IsoFilesystem *fs, IsoFileSource **root)
{
    if (fs == NULL || root == NULL) {
        return ISO_NULL_POINTER;
    }
    return iso_file_source_new_lfs(NULL, NULL, root);
}

static
int lfs_get_by_path(IsoFilesystem *fs, const char *path, IsoFileSource **file)
{
    int ret;
    IsoFileSource *src;
    struct stat info;
    char *ptr, *brk_info, *component;
    
    if (fs == NULL || path == NULL || file == NULL) {
        return ISO_NULL_POINTER;
    }
    
    /* 
     * first of all check that it is a valid path.
     */
    if (lstat(path, &info) != 0) {
        int err;

        /* error, choose an appropriate return code */
        switch (errno) {
        case EACCES:
            err = ISO_FILE_ACCESS_DENIED;
            break;
        case ENOTDIR:
        case ENAMETOOLONG:
        case ELOOP:
            err = ISO_FILE_BAD_PATH;
            break;
        case ENOENT:
            err = ISO_FILE_DOESNT_EXIST;
            break;
        case EFAULT:
        case ENOMEM:
            err = ISO_OUT_OF_MEM;
            break;
        default:
            err = ISO_FILE_ERROR;
            break;
        }
        return err;
    }
    
    /* ok, path is valid. create the file source */
    ret = lfs_get_root(fs, &src);
    if (ret < 0) {
        return ret;
    }
    if (!strcmp(path, "/")) {
        /* we are looking for root */
        *file = src;
        return ISO_SUCCESS;
    }

    ptr = strdup(path);
    if (ptr == NULL) {
        iso_file_source_unref(src);
        return ISO_OUT_OF_MEM;
    }
    
    component = strtok_r(ptr, "/", &brk_info);
    while (component) {
        IsoFileSource *child = NULL;
        if (!strcmp(component, ".")) {
            child = src;
        } else if (!strcmp(component, "..")) {
            child = ((_LocalFsFileSource*)src->data)->parent;
            iso_file_source_ref(child);
            iso_file_source_unref(src);
        } else {
            ret = iso_file_source_new_lfs(src, component, &child);
            iso_file_source_unref(src);
            if (ret < 0) {
                break;
            }
        }
        
        src = child;
        component = strtok_r(NULL, "/", &brk_info);
    }

    free(ptr);
    if (ret > 0) {
        *file = src;
    }
    return ret;
}

static
unsigned int lfs_get_id(IsoFilesystem *fs)
{
    return ISO_LOCAL_FS_ID;
}

static
int lfs_fs_open(IsoFilesystem *fs)
{
    /* open() operation is not needed */
    return ISO_SUCCESS;
}

static
int lfs_fs_close(IsoFilesystem *fs)
{
    /* close() operation is not needed */
    return ISO_SUCCESS;
}

static
void lfs_fs_free(IsoFilesystem *fs)
{
    lfs = NULL;
}

int iso_local_filesystem_new(IsoFilesystem **fs)
{
    if (fs == NULL) {
        return ISO_NULL_POINTER;
    }

    if (lfs != NULL) {
        /* just take a new ref */
        iso_filesystem_ref(lfs);
    } else {

        lfs = malloc(sizeof(IsoFilesystem));
        if (lfs == NULL) {
            return ISO_OUT_OF_MEM;
        }

        /* fill struct */
        strncpy(lfs->type, "file", 4);
        lfs->refcount = 1;
        lfs->version = 0;
        lfs->data = NULL; /* we don't need private data */
        lfs->get_root = lfs_get_root;
        lfs->get_by_path = lfs_get_by_path;
        lfs->get_id = lfs_get_id;
        lfs->open = lfs_fs_open;
        lfs->close = lfs_fs_close;
        lfs->free = lfs_fs_free;
    }
    *fs = lfs;
    return ISO_SUCCESS;
}
