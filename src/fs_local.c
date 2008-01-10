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

#include "fsource.h"
#include "error.h"
#include "util.h"

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
int iso_file_source_new_lfs(const char *path, IsoFileSource **src);

/*
 * We can share a local filesystem object, as it has no private atts.
 */
IsoFilesystem *lfs= NULL;

typedef struct
{
    char *path;
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
    return strdup(data->path);
}

static
char* lfs_get_name(IsoFileSource *src)
{
    char *name, *p;
    _LocalFsFileSource *data;
    data = src->data;
    p = strdup(data->path); /* because basename() might modify its arg */
    name = strdup(basename(p));
    free(p);
    return name;
}

static
int lfs_lstat(IsoFileSource *src, struct stat *info)
{
    _LocalFsFileSource *data;

    if (src == NULL || info == NULL) {
        return ISO_NULL_POINTER;
    }
    data = src->data;

    if (lstat(data->path, info) != 0) {
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
            err = ISO_MEM_ERROR;
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
int lfs_stat(IsoFileSource *src, struct stat *info)
{
    _LocalFsFileSource *data;

    if (src == NULL || info == NULL) {
        return ISO_NULL_POINTER;
    }
    data = src->data;

    if (stat(data->path, info) != 0) {
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
            err = ISO_MEM_ERROR;
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
int lfs_access(IsoFileSource *src)
{
    _LocalFsFileSource *data;

    if (src == NULL) {
        return ISO_NULL_POINTER;
    }
    data = src->data;

    return iso_eaccess(data->path);
}

static
int lfs_open(IsoFileSource *src)
{
    int err;
    struct stat info;
    _LocalFsFileSource *data;

    if (src == NULL) {
        return ISO_NULL_POINTER;
    }

    data = src->data;
    if (data->openned) {
        return ISO_FILE_ALREADY_OPENNED;
    }

    /* is a file or a dir ? */
    err = lfs_stat(src, &info);
    if (err < 0) {
        return err;
    }

    if (S_ISDIR(info.st_mode)) {
        data->info.dir = opendir(data->path);
        data->openned = data->info.dir ? 2 : 0;
    } else {
        data->info.fd = open(data->path, O_RDONLY);
        data->openned = data->info.fd != -1 ? 1 : 0;
    }

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
            err = ISO_MEM_ERROR;
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
        ret = ISO_FILE_NOT_OPENNED;
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
                    ret = ISO_MEM_ERROR;
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
        return ISO_FILE_NOT_OPENNED;
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
            char *path;
            struct dirent *entry;
            size_t a, b;
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

            /* constructs the new path */
            a = strlen(data->path);
            b = strlen(entry->d_name);
            path = malloc(a + b + 2);
            if (path == NULL) {
                return ISO_MEM_ERROR;
            }
            strncpy(path, data->path, a);
            path[a] = '/';
            path[a + 1] = '\0';
            strncat(path, entry->d_name, b);

            /* create the new FileSrc */
            ret = iso_file_source_new_lfs(path, child);
            free(path);
            return ret;
        }
    default:
        return ISO_FILE_NOT_OPENNED;
    }
}

static
int lfs_readlink(IsoFileSource *src, char *buf, size_t bufsiz)
{
    int size;
    _LocalFsFileSource *data;

    if (src == NULL || buf == NULL) {
        return ISO_NULL_POINTER;
    }

    if (bufsiz <= 0) {
        return ISO_WRONG_ARG_VALUE;
    }

    data = src->data;

    /*
     * invoke readlink, with bufsiz -1 to reserve an space for
     * the NULL character
     */
    size = readlink(data->path, buf, bufsiz - 1);
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
            return ISO_MEM_ERROR;
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

    free(data->path);
    free(data);
    iso_filesystem_unref(lfs);
}

IsoFileSourceIface lfs_class = { 
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
    lfs_free
};

/**
 * 
 * @return
 *     1 success, < 0 error
 */
static
int iso_file_source_new_lfs(const char *path, IsoFileSource **src)
{
    IsoFileSource *lfs_src;
    _LocalFsFileSource *data;

    if (src == NULL) {
        return ISO_NULL_POINTER;
    }

    if (lfs == NULL) {
        /* this should never happen */
        return ISO_ERROR;
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
    data->path = strdup(path);
    {
        /* remove trailing '/' */
        int len = strlen(path);
        if (len > 1) {
            /* don't remove / for root! */
            if (path[len-1] == '/') {
                data->path[len-1] = '\0';
            }
        }
    }
    data->openned = 0;

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
    return iso_file_source_new_lfs("/", root);
}

static
int lfs_get_by_path(IsoFilesystem *fs, const char *path, IsoFileSource **file)
{
    if (fs == NULL || path == NULL || file == NULL) {
        return ISO_NULL_POINTER;
    }
    return iso_file_source_new_lfs(path, file);
}

static
unsigned int lfs_get_id(IsoFilesystem *fs)
{
    return ISO_LOCAL_FS_ID;
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
        lfs->refcount = 1;
        lfs->data = NULL; /* we don't need private data */
        lfs->get_root = lfs_get_root;
        lfs->get_by_path = lfs_get_by_path;
        lfs->get_id = lfs_get_id;
        lfs->free = lfs_fs_free;
    }
    *fs = lfs;
    return ISO_SUCCESS;
}
