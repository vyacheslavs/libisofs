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
#include <time.h>

static
struct mock_file *path_to_node(IsoFilesystem *fs, const char *path);

static
char *get_path_aux(struct mock_file *file)
{
    if (file->parent == NULL) {
        return strdup("");
    } else {
        char *path = get_path_aux(file->parent);
        int pathlen = strlen(path);
        path = realloc(path, pathlen + strlen(file->name) + 2);
        path[pathlen] = '/';
        path[pathlen + 1] = '\0';
        return strcat(path, file->name);
    }
}

static
char* mfs_get_path(IsoFileSource *src)
{
    struct mock_file *data;
    data = src->data;
    return get_path_aux(data);
}

static
char* mfs_get_name(IsoFileSource *src)
{
    struct mock_file *data;
    data = src->data;
    return strdup(data->name);
}
    
static
int mfs_lstat(IsoFileSource *src, struct stat *info)
{
    struct mock_file *data;

    if (src == NULL || info == NULL) {
        return ISO_NULL_POINTER;
    }
    data = src->data;
    
    *info = data->atts;
    return ISO_SUCCESS;
}
    
static
int mfs_stat(IsoFileSource *src, struct stat *info)
{
    struct mock_file *node;
    if (src == NULL || info == NULL) {
        return ISO_NULL_POINTER;
    }
    node = src->data;
    
    while ( S_ISLNK(node->atts.st_mode) ) {
        /* the destination is stated */
        node = path_to_node(node->fs, (char *)node->content);
        if (node == NULL) {
            return ISO_FILE_ERROR;
        }
    }

    *info = node->atts;
    return ISO_SUCCESS;
}

static
int mfs_access(IsoFileSource *src)
{
    // TODO not implemented
    return ISO_ERROR;
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
    struct mock_file *data;

    if (src == NULL || buf == NULL) {
        return ISO_NULL_POINTER;
    }

    if (bufsiz <= 0) {
        return ISO_WRONG_ARG_VALUE;
    }
    data = src->data;
    
    if (!S_ISLNK(data->atts.st_mode)) {
        return ISO_FILE_IS_NOT_SYMLINK;
    }
    strncpy(buf, data->content, bufsiz);
    buf[bufsiz-1] = '\0';
    return ISO_SUCCESS;
}

static
IsoFilesystem* mfs_get_filesystem(IsoFileSource *src)
{
    struct mock_file *data;
    data = src->data;
    return data->fs;
}

static
void mfs_free(IsoFileSource *src)
{
    /* nothing to do */
}

IsoFileSourceIface mfs_class = {
    mfs_get_path,
    mfs_get_name,
    mfs_lstat,
    mfs_stat,
    mfs_access,
    mfs_open,
    mfs_close,
    mfs_read,
    mfs_readdir,
    mfs_readlink,
    mfs_get_filesystem,
    mfs_free
};

/**
 * 
 * @return
 *     1 success, < 0 error
 */
static
int mocked_file_source_new(struct mock_file *data, IsoFileSource **src)
{
    IsoFileSource *mocked_src;

    if (src == NULL || data == NULL) {
        return ISO_NULL_POINTER;
    }
    
    /* allocate memory */
    mocked_src = malloc(sizeof(IsoFileSource));
    if (mocked_src == NULL) {
        free(data);
        return ISO_OUT_OF_MEM;
    }

    /* fill struct */
    mocked_src->refcount = 1;
    mocked_src->data = data;
    mocked_src->class = &mfs_class;
    
    /* take a ref to filesystem */
    //iso_filesystem_ref(fs);

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
        
        if ( !S_ISDIR(node->atts.st_mode) ) {
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
void add_node(struct mock_file *parent, struct mock_file *node)
{
    int i;

    i = 0;
    if (parent->content) {
        while (((struct mock_file**)parent->content)[i]) {
            ++i;
        }
    }
    parent->content = realloc(parent->content, (i+2) * sizeof(void*));
    ((struct mock_file**)parent->content)[i] = node;
    ((struct mock_file**)parent->content)[i+1] = NULL;
}

struct mock_file *test_mocked_fs_get_root(IsoFilesystem *fs)
{
    return fs->data;
}

int test_mocked_fs_add_dir(const char *name, struct mock_file *p, 
                           struct stat atts, struct mock_file **node)
{
    struct mock_file *dir = calloc(1, sizeof(struct mock_file));
    dir->fs = p->fs;
    dir->atts = atts;
    dir->name = strdup(name);
    dir->parent = p;
    add_node(p, dir);
    
    *node = dir;
    return ISO_SUCCESS;
}

int test_mocked_fs_add_symlink(const char *name, struct mock_file *p, 
          struct stat atts, const char *dest, struct mock_file **node)
{
    struct mock_file *link = calloc(1, sizeof(struct mock_file));
    link->fs = p->fs;
    link->atts = atts;
    link->name = strdup(name);
    link->parent = p;
    add_node(p, link);
    link->content = strdup(dest);
    *node = link;
    return ISO_SUCCESS;
}

static
int mocked_get_root(IsoFilesystem *fs, IsoFileSource **root)
{
    if (fs == NULL || root == NULL) {
        return ISO_NULL_POINTER;
    }
    return mocked_file_source_new(fs->data, root);
}

static
int mocked_get_by_path(IsoFilesystem *fs, const char *path, IsoFileSource **file)
{
    struct mock_file *f;
    if (fs == NULL || path == NULL || file == NULL) {
        return ISO_NULL_POINTER;
    }
    f = path_to_node(fs, path);
    return mocked_file_source_new(f, file);
}

static
void free_mocked_file(struct mock_file *file)
{
    if (S_ISDIR(file->atts.st_mode)) {
        if (file->content) {
            int i = 0;
            while (((struct mock_file**)file->content)[i]) {
                free_mocked_file(((struct mock_file**)file->content)[i]);
                ++i;
            }
        }
    }
    free(file->content);
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
    struct mock_file *root;
    IsoFilesystem *filesystem;
    
    if (fs == NULL) {
        return ISO_NULL_POINTER;
    }
    
    root = calloc(1, sizeof(struct mock_file));
    root->atts.st_atime = time(NULL);
    root->atts.st_ctime = time(NULL);
    root->atts.st_mtime = time(NULL);
    root->atts.st_uid = 0;
    root->atts.st_gid = 0;
    root->atts.st_mode = S_IFDIR | 0777;
        
    filesystem = malloc(sizeof(IsoFilesystem));
    filesystem->refcount = 1;
    root->fs = filesystem;
    filesystem->data = root;
    filesystem->get_root = mocked_get_root;
    filesystem->get_by_path = mocked_get_by_path;
    filesystem->free = mocked_fs_free;
    *fs = filesystem;
    return ISO_SUCCESS;
}

