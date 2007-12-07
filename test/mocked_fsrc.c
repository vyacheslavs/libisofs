/*
 * 
 * 
 */
 
#include "fsource.h"
#include "mocked_fsrc.h"
#include "error.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <stdlib.h>

static
struct mock_file *path_to_node(IsoFilesystem *fs, const char *path);

struct mock_file {
    struct stat info;
    char *name;
    void *content;
};

struct mock_fs_src
{
    IsoFilesystem *fs;
    char *path;
} _MockedFsFileSource;

static
const char* mfs_get_path(IsoFileSource *src)
{
    struct mock_fs_src *data;
    data = src->data;
    return data->path;
}

static
char* mfs_get_name(IsoFileSource *src)
{
    char *name, *p;
    struct mock_fs_src *data;
    data = src->data;
    p = strdup(data->path); /* because basename() might modify its arg */
    name = strdup(basename(p));
    free(p);    
    return name;
}
    
static
int mfs_lstat(IsoFileSource *src, struct stat *info)
{
    struct mock_fs_src *data;
    struct mock_file *node;

    if (src == NULL || info == NULL) {
        return ISO_NULL_POINTER;
    }
    data = src->data;
    
    node = path_to_node(data->fs, data->path);
    if (node == NULL) {
        return ISO_FILE_BAD_PATH;
    }
    
    *info = node->info;
    return ISO_SUCCESS;
}
    
static
int mfs_stat(IsoFileSource *src, struct stat *info)
{
    struct mock_fs_src *data;
    struct mock_file *node;

    if (src == NULL || info == NULL) {
        return ISO_NULL_POINTER;
    }
    data = src->data;
    
    node = path_to_node(data->fs, data->path);
    if (node == NULL) {
        return ISO_FILE_BAD_PATH;
    }
    
    while ( S_ISLNK(node->info.st_mode) ) {
        /* the destination is stated */
        node = path_to_node(data->fs, (char *)node->content);
        if (node == NULL) {
            return ISO_FILE_ERROR;
        }
    }
    
    *info = node->info;
    return ISO_SUCCESS;
}

static
int mfs_open(IsoFileSource *src)
{
    // TODO not implemented
    return ISO_ERROR;
}

static
int mfs_close(IsoFileSource *src)
{
    // TODO not implemented
    return ISO_ERROR;
}

static
int mfs_read(IsoFileSource *src, void *buf, size_t count)
{
    // TODO not implemented
    return ISO_ERROR;
}

static
int mfs_readdir(IsoFileSource *src, IsoFileSource **child)
{
    // TODO not implemented
    return ISO_ERROR;
}

static
int mfs_readlink(IsoFileSource *src, char *buf, size_t bufsiz)
{
    struct mock_fs_src *data;
    struct mock_file *node;

    if (src == NULL || buf == NULL) {
        return ISO_NULL_POINTER;
    }

    if (bufsiz <= 0) {
        return ISO_WRONG_ARG_VALUE;
    }
    data = src->data;
    
    node = path_to_node(data->fs, data->path);
    if (node == NULL) {
        return ISO_FILE_BAD_PATH;
    }
    if (!S_ISLNK(node->info.st_mode)) {
        return ISO_FILE_IS_NOT_SYMLINK;
    }
    strncpy(buf, node->content, bufsiz);
    buf[bufsiz-1] = '\0';
    return ISO_SUCCESS;
}

static
IsoFilesystem* mfs_get_filesystem(IsoFileSource *src)
{
    struct mock_fs_src *data;
    data = src->data;
    return data->fs;
}

static
void mfs_free(IsoFileSource *src)
{
    struct mock_fs_src *data;

    data = src->data;
    free(data->path);
    iso_filesystem_unref(data->fs);
    free(data);
}

/**
 * 
 * @return
 *     1 success, < 0 error
 */
static
int mocked_file_source_new(IsoFilesystem *fs, const char *path, 
                           IsoFileSource **src)
{
    IsoFileSource *mocked_src;
    struct mock_fs_src *data;

    if (src == NULL || fs == NULL) {
        return ISO_NULL_POINTER;
    }
    
    /* allocate memory */
    data = malloc(sizeof(struct mock_fs_src));
    if (data == NULL) {
        return ISO_OUT_OF_MEM;
    }
    mocked_src = malloc(sizeof(IsoFileSource));
    if (mocked_src == NULL) {
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
    data->fs = fs;

    mocked_src->refcount = 1;
    mocked_src->data = data;
    mocked_src->get_path = mfs_get_path;
    mocked_src->get_name = mfs_get_name;
    mocked_src->lstat = mfs_lstat;
    mocked_src->stat = mfs_stat;
    mocked_src->open = mfs_open;
    mocked_src->close = mfs_close;
    mocked_src->read = mfs_read;
    mocked_src->readdir = mfs_readdir;
    mocked_src->readlink = mfs_readlink;
    mocked_src->get_filesystem = mfs_get_filesystem;
    mocked_src->free = mfs_free;
    
    /* take a ref to filesystem */
    iso_filesystem_ref(fs);

    /* return */
    *src = mocked_src;
    return ISO_SUCCESS;
}

static
struct mock_file *path_to_node(IsoFilesystem *fs, const char *path)
{
    struct mock_file *node;
    struct mock_file *dir;
    char *ptr, *brk_info, *component;

    /* get the first child at the root of the volume
     * that is "/" */
    dir = fs->data;
    node = dir;
    if (!strcmp(path, "/"))
        return node;

    ptr = strdup(path);

    /* get the first component of the path */
    component = strtok_r(ptr, "/", &brk_info);
    while (component) {
        size_t i;
        
        if ( !S_ISDIR(node->info.st_mode) ) {
            node=NULL;
            break;
        }
        dir = node;
        
        node=NULL;
        if (!dir->content) {
            break;
        }

        i = 0;
        while (((struct mock_file**)dir->content)[i]) {
            if (!strcmp(component, ((struct mock_file**)dir->content)[i]->name)) {
                node = ((struct mock_file**)dir->content)[i];
                break;
            }
            ++i;
        }

        /* see if a node could be found */
        if (node==NULL) {
            break;
        }
        component = strtok_r(NULL, "/", &brk_info);
    }
    free(ptr);
    return node;
}

static
struct mock_file *add_node(IsoFilesystem *fs, const char *path, 
                            struct stat info)
{
    char *dir, *name, *ptr;
    struct mock_file *node, *parent;
    int i;

    if (!strcmp(path, "/"))
        return NULL;
    
    dir = dirname(strdup(path));
    ptr = strdup(path);
    name = basename(ptr);
    /*printf("Added %s to %s\n", name, dir);*/
    
    parent = path_to_node(fs, dir);
    if (!S_ISDIR(parent->info.st_mode)) {
        return NULL;
    }
    i = 0;
    if (parent->content) {
        while (((struct mock_file**)parent->content)[i]) {
            ++i;
        }
    }
    parent->content = realloc(parent->content, (i+2) * sizeof(struct mock_file*));
    node = malloc(sizeof(struct mock_file));
    node->info = info;
    node->content = NULL;
    node->name = strdup(name);
    ((struct mock_file**)parent->content)[i] = node;
    ((struct mock_file**)parent->content)[i+1] = NULL;

    free(dir);
    free(ptr);
    return node;
}

int test_mocked_fs_add_file(IsoFilesystem *fs, const char *path, 
                            struct stat info)
{
    struct mock_file *node = add_node(fs, path, info);
    return (node == NULL) ? -1 : 1;
}

int test_mocked_fs_add_dir(IsoFilesystem *fs, const char *path, 
                            struct stat info)
{
    struct mock_file *node = add_node(fs, path, info);
    return (node == NULL) ? -1 : 1;
}

int test_mocked_fs_add_symlink(IsoFilesystem *fs, const char *path, 
                            struct stat info, const char *dest)
{
    struct mock_file *node = add_node(fs, path, info);
    if (node == NULL) {
        return -1;
    }
    node->content = strdup(dest);
    return 1;
}

static
int mocked_get_root(IsoFilesystem *fs, IsoFileSource **root)
{
    if (fs == NULL || root == NULL) {
        return ISO_NULL_POINTER;
    }
    return mocked_file_source_new(fs, "/", root);
}

static
int mocked_get_by_path(IsoFilesystem *fs, const char *path, IsoFileSource **file)
{
    if (fs == NULL || path == NULL || file == NULL) {
        return ISO_NULL_POINTER;
    }
    return mocked_file_source_new(fs, path, file);
}

static
void free_mocked_file(struct mock_file *file)
{
    if (S_ISDIR(file->info.st_mode)) {
        if (file->content) {
            int i = 0;
            while (((struct mock_file**)file->content)[i]) {
                free_mocked_file(((struct mock_file**)file->content)[i]);
                ++i;
            }
        }
    }
    free(file->name);
    free(file);
}

static
void mocked_fs_free(IsoFilesystem *fs)
{
    free_mocked_file((struct mock_file *)fs->data);
}
    
int test_mocked_filesystem_new(IsoFilesystem **fs)
{
    IsoFilesystem *filesystem;
    struct mock_file *root;
    if (fs == NULL) {
        return ISO_NULL_POINTER;
    }
    
    filesystem = malloc(sizeof(IsoFilesystem));
    filesystem->refcount = 1;
    root = calloc(1, sizeof(struct mock_file));
    root->info.st_mode = S_IFDIR | 0777;
    filesystem->data = root;
    filesystem->get_root = mocked_get_root;
    filesystem->get_by_path = mocked_get_by_path;
    filesystem->free = mocked_fs_free;
    *fs = filesystem;
    return ISO_SUCCESS;
}

