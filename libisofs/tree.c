/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 */

/*
 * Functions that act on the iso tree.
 */

#include "libisofs.h"
#include "node.h"
#include "image.h"
#include "fsource.h"
#include "builder.h"
#include "messages.h"
#include "tree.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <stdio.h>
#include <fnmatch.h>


#ifndef PATH_MAX
#define PATH_MAX Libisofs_default_path_maX
#endif


/**
 * Add a new directory to the iso tree.
 * 
 * @param parent 
 *      the dir where the new directory will be created
 * @param name
 *      name for the new dir. If a node with same name already exists on
 *      parent, this functions fails with ISO_NODE_NAME_NOT_UNIQUE.
 * @param dir
 *      place where to store a pointer to the newly created dir. No extra
 *      ref is addded, so you will need to call iso_node_ref() if you really
 *      need it. You can pass NULL in this parameter if you don't need the
 *      pointer.
 * @return
 *     number of nodes in dir if succes, < 0 otherwise
 *     Possible errors:
 *         ISO_NULL_POINTER, if parent or name are NULL
 *         ISO_NODE_NAME_NOT_UNIQUE, a node with same name already exists
 */
int iso_tree_add_new_dir(IsoDir *parent, const char *name, IsoDir **dir)
{
    int ret;
    char *n;
    IsoDir *node;
    IsoNode **pos;
    time_t now;

    if (parent == NULL || name == NULL) {
        return ISO_NULL_POINTER;
    }
    if (dir) {
        *dir = NULL;
    }

    /* find place where to insert and check if it exists */
    if (iso_dir_exists(parent, name, &pos)) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    n = strdup(name);
    ret = iso_node_new_dir(n, &node);
    if (ret < 0) {
        free(n);
        return ret;
    }

    /* permissions from parent */
    iso_node_set_permissions((IsoNode*)node, parent->node.mode);
    iso_node_set_uid((IsoNode*)node, parent->node.uid);
    iso_node_set_gid((IsoNode*)node, parent->node.gid);
    iso_node_set_hidden((IsoNode*)node, parent->node.hidden);

    /* current time */
    now = time(NULL);
    iso_node_set_atime((IsoNode*)node, now);
    iso_node_set_ctime((IsoNode*)node, now);
    iso_node_set_mtime((IsoNode*)node, now);

    if (dir) {
        *dir = node;
    }

    /* add to dir */
    return iso_dir_insert(parent, (IsoNode*)node, pos, ISO_REPLACE_NEVER);
}

/**
 * Add a new symlink to the directory tree. Permissions are set to 0777, 
 * owner and hidden atts are taken from parent. You can modify any of them 
 * later.
 *  
 * @param parent 
 *      the dir where the new symlink will be created
 * @param name
 *      name for the new dir. If a node with same name already exists on
 *      parent, this functions fails with ISO_NODE_NAME_NOT_UNIQUE.
 * @param dest
 *      destination of the link
 * @param link
 *      place where to store a pointer to the newly created link. No extra
 *      ref is addded, so you will need to call iso_node_ref() if you really
 *      need it. You can pass NULL in this parameter if you don't need the
 *      pointer
 * @return
 *     number of nodes in parent if success, < 0 otherwise
 *     Possible errors:
 *         ISO_NULL_POINTER, if parent, name or dest are NULL
 *         ISO_NODE_NAME_NOT_UNIQUE, a node with same name already exists
 *         ISO_OUT_OF_MEM
 */
int iso_tree_add_new_symlink(IsoDir *parent, const char *name,
                             const char *dest, IsoSymlink **link)
{
    int ret;
    char *n, *d;
    IsoSymlink *node;
    IsoNode **pos;
    time_t now;

    if (parent == NULL || name == NULL || dest == NULL) {
        return ISO_NULL_POINTER;
    }
    if (link) {
        *link = NULL;
    }

    /* find place where to insert */
    if (iso_dir_exists(parent, name, &pos)) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    n = strdup(name);
    d = strdup(dest);
    ret = iso_node_new_symlink(n, d, &node);
    if (ret < 0) {
        free(n);
        free(d);
        return ret;
    }

    /* permissions from parent */
    iso_node_set_permissions((IsoNode*)node, 0777);
    iso_node_set_uid((IsoNode*)node, parent->node.uid);
    iso_node_set_gid((IsoNode*)node, parent->node.gid);
    iso_node_set_hidden((IsoNode*)node, parent->node.hidden);

    /* current time */
    now = time(NULL);
    iso_node_set_atime((IsoNode*)node, now);
    iso_node_set_ctime((IsoNode*)node, now);
    iso_node_set_mtime((IsoNode*)node, now);

    if (link) {
        *link = node;
    }

    /* add to dir */
    return iso_dir_insert(parent, (IsoNode*)node, pos, ISO_REPLACE_NEVER);
}

/**
 * Add a new special file to the directory tree. As far as libisofs concerns,
 * an special file is a block device, a character device, a FIFO (named pipe)
 * or a socket. You can choose the specific kind of file you want to add
 * by setting mode propertly (see man 2 stat).
 * 
 * Note that special files are only written to image when Rock Ridge 
 * extensions are enabled. Moreover, a special file is just a directory entry
 * in the image tree, no data is written beyond that.
 * 
 * Owner and hidden atts are taken from parent. You can modify any of them 
 * later.
 * 
 * @param parent
 *      the dir where the new special file will be created
 * @param name
 *      name for the new special file. If a node with same name already exists 
 *      on parent, this functions fails with ISO_NODE_NAME_NOT_UNIQUE.
 * @param mode
 *      file type and permissions for the new node. Note that you can't
 *      specify any kind of file here, only special types are allowed. i.e,
 *      S_IFSOCK, S_IFBLK, S_IFCHR and S_IFIFO are valid types; S_IFLNK, 
 *      S_IFREG and S_IFDIR aren't.
 * @param dev
 *      device ID, equivalent to the st_rdev field in man 2 stat.
 * @param special
 *      place where to store a pointer to the newly created special file. No 
 *      extra ref is addded, so you will need to call iso_node_ref() if you 
 *      really need it. You can pass NULL in this parameter if you don't need 
 *      the pointer.
 * @return
 *     number of nodes in parent if success, < 0 otherwise
 *     Possible errors:
 *         ISO_NULL_POINTER, if parent, name or dest are NULL
 *         ISO_NODE_NAME_NOT_UNIQUE, a node with same name already exists
 *         ISO_OUT_OF_MEM
 * 
 */
int iso_tree_add_new_special(IsoDir *parent, const char *name, mode_t mode,
                             dev_t dev, IsoSpecial **special)
{
    int ret;
    char *n;
    IsoSpecial *node;
    IsoNode **pos;
    time_t now;

    if (parent == NULL || name == NULL) {
        return ISO_NULL_POINTER;
    }
    if (S_ISLNK(mode) || S_ISREG(mode) || S_ISDIR(mode)) {
        return ISO_WRONG_ARG_VALUE;
    }
    if (special) {
        *special = NULL;
    }

    /* find place where to insert */
    if (iso_dir_exists(parent, name, &pos)) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    n = strdup(name);
    ret = iso_node_new_special(n, mode, dev, &node);
    if (ret < 0) {
        free(n);
        return ret;
    }

    /* atts from parent */
    iso_node_set_uid((IsoNode*)node, parent->node.uid);
    iso_node_set_gid((IsoNode*)node, parent->node.gid);
    iso_node_set_hidden((IsoNode*)node, parent->node.hidden);

    /* current time */
    now = time(NULL);
    iso_node_set_atime((IsoNode*)node, now);
    iso_node_set_ctime((IsoNode*)node, now);
    iso_node_set_mtime((IsoNode*)node, now);

    if (special) {
        *special = node;
    }

    /* add to dir */
    return iso_dir_insert(parent, (IsoNode*)node, pos, ISO_REPLACE_NEVER);
}

/**
 * Add a new regular file to the iso tree. Permissions are set to 0444, 
 * owner and hidden atts are taken from parent. You can modify any of them 
 * later.
 *  
 * @param parent 
 *      the dir where the new file will be created
 * @param name
 *      name for the new file. If a node with same name already exists on
 *      parent, this functions fails with ISO_NODE_NAME_NOT_UNIQUE.
 * @param stream
 *      IsoStream for the contents of the file
 * @param file
 *      place where to store a pointer to the newly created file. No extra
 *      ref is addded, so you will need to call iso_node_ref() if you really
 *      need it. You can pass NULL in this parameter if you don't need the
 *      pointer
 * @return
 *     number of nodes in parent if success, < 0 otherwise
 *     Possible errors:
 *         ISO_NULL_POINTER, if parent, name or dest are NULL
 *         ISO_NODE_NAME_NOT_UNIQUE, a node with same name already exists
 *         ISO_OUT_OF_MEM
 * 
 * @since 0.6.4
 */
int iso_tree_add_new_file(IsoDir *parent, const char *name, IsoStream *stream, 
                          IsoFile **file)
{
    int ret;
    char *n;
    IsoFile *node;
    IsoNode **pos;
    time_t now;

    if (parent == NULL || name == NULL || stream == NULL) {
        return ISO_NULL_POINTER;
    }
    if (file) {
        *file = NULL;
    }

    /* find place where to insert */
    if (iso_dir_exists(parent, name, &pos)) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    n = strdup(name);
    ret = iso_node_new_file(n, stream, &node);
    if (ret < 0) {
        free(n);
        return ret;
    }

    /* permissions from parent */
    iso_node_set_permissions((IsoNode*)node, 0444);
    iso_node_set_uid((IsoNode*)node, parent->node.uid);
    iso_node_set_gid((IsoNode*)node, parent->node.gid);
    iso_node_set_hidden((IsoNode*)node, parent->node.hidden);

    /* current time */
    now = time(NULL);
    iso_node_set_atime((IsoNode*)node, now);
    iso_node_set_ctime((IsoNode*)node, now);
    iso_node_set_mtime((IsoNode*)node, now);

    if (file) {
        *file = node;
    }

    /* add to dir */
    return iso_dir_insert(parent, (IsoNode*)node, pos, ISO_REPLACE_NEVER);
}

/**
 * Set whether to follow or not symbolic links when added a file from a source
 * to IsoImage.
 */
void iso_tree_set_follow_symlinks(IsoImage *image, int follow)
{
    image->follow_symlinks = follow ? 1 : 0;
}

/**
 * Get current setting for follow_symlinks.
 * 
 * @see iso_tree_set_follow_symlinks
 */
int iso_tree_get_follow_symlinks(IsoImage *image)
{
    return image->follow_symlinks;
}

/**
 * Set whether to skip or not hidden files when adding a directory recursibely.
 * Default behavior is to not ignore them, i.e., to add hidden files to image.
 */
void iso_tree_set_ignore_hidden(IsoImage *image, int skip)
{
    image->ignore_hidden = skip ? 1 : 0;
}

/**
 * Get current setting for ignore_hidden.
 * 
 * @see iso_tree_set_ignore_hidden
 */
int iso_tree_get_ignore_hidden(IsoImage *image)
{
    return image->ignore_hidden;
}

void iso_tree_set_replace_mode(IsoImage *image, enum iso_replace_mode mode)
{
    image->replace = mode;
}

enum iso_replace_mode iso_tree_get_replace_mode(IsoImage *image)
{
    return image->replace;
}

/**
 * Set whether to skip or not special files. Default behavior is to not skip
 * them. Note that, despite of this setting, special files won't never be added
 * to an image unless RR extensions were enabled.
 * 
 * @param skip 
 *      Bitmask to determine what kind of special files will be skipped:
 *          bit0: ignore FIFOs
 *          bit1: ignore Sockets
 *          bit2: ignore char devices
 *          bit3: ignore block devices
 */
void iso_tree_set_ignore_special(IsoImage *image, int skip)
{
    image->ignore_special = skip & 0x0F;
}

/**
 * Get current setting for ignore_special.
 * 
 * @see iso_tree_set_ignore_special
 */
int iso_tree_get_ignore_special(IsoImage *image)
{
    return image->ignore_special;
}

/**
 * Set a callback function that libisofs will call for each file that is
 * added to the given image by a recursive addition function. This includes
 * image import.
 *  
 * @param report
 *      pointer to a function that will be called just before a file will be 
 *      added to the image. You can control whether the file will be in fact 
 *      added or ignored.
 *      This function should return 1 to add the file, 0 to ignore it and 
 *      continue, < 0 to abort the process
 *      NULL is allowed if you don't want any callback.
 */
void iso_tree_set_report_callback(IsoImage *image, 
                                  int (*report)(IsoImage*, IsoFileSource*))
{
    image->report = report;
}

/**
 * Add a excluded path. These are paths that won't never added to image,
 * and will be excluded even when adding recursively its parent directory.
 * 
 * For example, in
 * 
 * iso_tree_add_exclude(image, "/home/user/data/private");
 * iso_tree_add_dir_rec(image, root, "/home/user/data");
 * 
 * the directory /home/user/data/private won't be added to image.
 * 
 * @return
 *      1 on success, < 0 on error
 */
int iso_tree_add_exclude(IsoImage *image, const char *path)
{
    if (image == NULL || path == NULL) {
        return ISO_NULL_POINTER;
    }
    image->excludes = realloc(image->excludes, ++image->nexcludes * 
                              sizeof(void*));
    if (image->excludes == NULL) {
        return ISO_OUT_OF_MEM;
    }
    image->excludes[image->nexcludes - 1] = strdup(path);
    if (image->excludes[image->nexcludes - 1] == NULL) {
        return ISO_OUT_OF_MEM;
    }
    return ISO_SUCCESS;
}

/**
 * Remove a previously added exclude.
 * 
 * @see iso_tree_add_exclude
 * @return
 *      1 on success, 0 exclude do not exists, < 0 on error
 */
int iso_tree_remove_exclude(IsoImage *image, const char *path)
{
    size_t i, j;

    if (image == NULL || path == NULL) {
        return ISO_NULL_POINTER;
    }

    for (i = 0; i < image->nexcludes; ++i) {
        if (strcmp(image->excludes[i], path) == 0) {
            /* exclude found */
            free(image->excludes[i]);
            --image->nexcludes;
            for (j = i; j < image->nexcludes; ++j) {
                image->excludes[j] = image->excludes[j+1]; 
            }
            image->excludes = realloc(image->excludes, image->nexcludes * 
                                      sizeof(void*));
            return ISO_SUCCESS;
        }
    }
    return 0;
}

static
int iso_tree_add_node_builder(IsoImage *image, IsoDir *parent,
                              IsoFileSource *src, IsoNodeBuilder *builder,
                              IsoNode **node)
{
    int result;
    IsoNode *new;
    IsoNode **pos;
    char *name;

    if (parent == NULL || src == NULL || builder == NULL) {
        return ISO_NULL_POINTER;
    }
    if (node) {
        *node = NULL;
    }

    name = iso_file_source_get_name(src);

    /* find place where to insert */
    result = iso_dir_exists(parent, name, &pos);
    free(name);
    if (result) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    result = builder->create_node(builder, image, src, &new);
    if (result < 0) {
        return result;
    }

    if (node) {
        *node = new;
    }

    /* finally, add node to parent */
    return iso_dir_insert(parent, (IsoNode*)new, pos, ISO_REPLACE_NEVER);
}

int iso_tree_add_node(IsoImage *image, IsoDir *parent, const char *path,
                      IsoNode **node)
{
    int result;
    IsoFilesystem *fs;
    IsoFileSource *file;

    if (image == NULL || parent == NULL || path == NULL) {
        return ISO_NULL_POINTER;
    }

    fs = image->fs;
    result = fs->get_by_path(fs, path, &file);
    if (result < 0) {
        return result;
    }
    result = iso_tree_add_node_builder(image, parent, file, image->builder,
                                       node);
    /* free the file */
    iso_file_source_unref(file);
    return result;
}

int iso_tree_add_new_node(IsoImage *image, IsoDir *parent, const char *name, 
                          const char *path, IsoNode **node)
{
    int result;
    IsoFilesystem *fs;
    IsoFileSource *file;
    IsoNode *new;
    IsoNode **pos;

    if (image == NULL || parent == NULL || name == NULL || path == NULL) {
        return ISO_NULL_POINTER;
    }

    if (node) {
        *node = NULL;
    }

    fs = image->fs;
    result = fs->get_by_path(fs, path, &file);
    if (result < 0) {
        return result;
    }

    /* find place where to insert */
    result = iso_dir_exists(parent, name, &pos);
    if (result) {
        /* a node with same name already exists */
        iso_file_source_unref(file);
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    result = image->builder->create_node(image->builder, image, file, &new);
    
    /* free the file */
    iso_file_source_unref(file);
    
    if (result < 0) {
        return result;
    }
    
    result = iso_node_set_name(new, name);
    if (result < 0) {
        iso_node_unref(new);
        return result;
    }

    if (node) {
        *node = new;
    }

    /* finally, add node to parent */
    return iso_dir_insert(parent, new, pos, ISO_REPLACE_NEVER);
}

int iso_tree_add_new_cut_out_node(IsoImage *image, IsoDir *parent, 
                                  const char *name, const char *path, 
                                  off_t offset, off_t size,
                                  IsoNode **node)
{
    int result;
    struct stat info;
    IsoFilesystem *fs;
    IsoFileSource *src;
    IsoFile *new;
    IsoNode **pos;
    IsoStream *stream;

    if (image == NULL || parent == NULL || name == NULL || path == NULL) {
        return ISO_NULL_POINTER;
    }

    if (node) {
        *node = NULL;
    }

    /* find place where to insert */
    result = iso_dir_exists(parent, name, &pos);
    if (result) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    fs = image->fs;
    result = fs->get_by_path(fs, path, &src);
    if (result < 0) {
        return result;
    }

    result = iso_file_source_stat(src, &info);
    if (result < 0) {
        iso_file_source_unref(src);
        return result;
    }
    if (!S_ISREG(info.st_mode)) {
        return ISO_WRONG_ARG_VALUE;
    }
    if (offset >= info.st_size) {
        return ISO_WRONG_ARG_VALUE;
    }

    /* force regular file */
    result = image->builder->create_file(image->builder, image, src, &new);
    
    /* free the file */
    iso_file_source_unref(src);
    
    if (result < 0) {
        return result;
    }
    
    /* replace file iso stream with a cut-out-stream */
    result = iso_cut_out_stream_new(src, offset, size, &stream);
    if (result < 0) {
        iso_node_unref((IsoNode*)new);
        return result;
    }
    iso_stream_unref(new->stream);
    new->stream = stream;
    
    result = iso_node_set_name((IsoNode*)new, name);
    if (result < 0) {
        iso_node_unref((IsoNode*)new);
        return result;
    }

    if (node) {
        *node = (IsoNode*)new;
    }

    /* finally, add node to parent */
    return iso_dir_insert(parent, (IsoNode*)new, pos, ISO_REPLACE_NEVER);
}

static
int check_excludes(IsoImage *image, const char *path)
{
    int i;

    for (i = 0; i < image->nexcludes; ++i) {
        char *exclude = image->excludes[i];
        if (exclude[0] == '/') {
            /* absolute exclude, must completely match path */
            if (!fnmatch(exclude, path, FNM_PERIOD|FNM_PATHNAME)) {
                return 1;
            }
        } else {
            /* relative exclude, it is enought if a part of the path matches */
            char *pos = (char*)path;
            while (pos != NULL) {
                pos++;
                if (!fnmatch(exclude, pos, FNM_PERIOD|FNM_PATHNAME)) {
                    return 1;
                }
                pos = strchr(pos, '/');
            }
        }
    }
    return 0;
}

static
int check_hidden(IsoImage *image, const char *name)
{
    return (image->ignore_hidden && name[0] == '.');
}

static
int check_special(IsoImage *image, mode_t mode)
{
    if (image->ignore_special != 0) {
        switch(mode &  S_IFMT) {
        case S_IFBLK:
            return image->ignore_special & 0x08 ? 1 : 0;
        case S_IFCHR:
            return image->ignore_special & 0x04 ? 1 : 0;
        case S_IFSOCK:
            return image->ignore_special & 0x02 ? 1 : 0;
        case S_IFIFO:
            return image->ignore_special & 0x01 ? 1 : 0;
        default:
            return 0;
        }
    }
    return 0;
}

/**
 * Recursively add a given directory to the image tree.
 * 
 * @return
 *      1 continue, < 0 error (ISO_CANCELED stop)
 */
int iso_add_dir_src_rec(IsoImage *image, IsoDir *parent, IsoFileSource *dir)
{
    int ret;
    IsoNodeBuilder *builder;
    IsoFileSource *file;
    IsoNode **pos;
    struct stat info;
    char *name, *path;
    IsoNode *new;
    enum iso_replace_mode replace;

    ret = iso_file_source_open(dir);
    if (ret < 0) {
        char *path = iso_file_source_get_path(dir);
        /* instead of the probable error, we throw a sorry event */
        ret = iso_msg_submit(image->id, ISO_FILE_CANT_ADD, ret, 
                             "Can't open dir %s", path);
        free(path);
        return ret;
    }

    builder = image->builder;
    
    /* iterate over all directory children */
    while (1) {
        int skip = 0;

        ret = iso_file_source_readdir(dir, &file);
        if (ret <= 0) {
            if (ret < 0) {
                /* error reading dir */
                ret = iso_msg_submit(image->id, ret, ret, "Error reading dir");
            }
            break;
        }

        path = iso_file_source_get_path(file);
        name = strrchr(path, '/') + 1;

        if (image->follow_symlinks) {
            ret = iso_file_source_stat(file, &info);
        } else {
            ret = iso_file_source_lstat(file, &info);
        }
        if (ret < 0) {
            goto dir_rec_continue;
        }

        if (check_excludes(image, path)) {
            iso_msg_debug(image->id, "Skipping excluded file %s", path);
            skip = 1;
        } else if (check_hidden(image, name)) {
            iso_msg_debug(image->id, "Skipping hidden file %s", path);
            skip = 1;
        } else if (check_special(image, info.st_mode)) {
            iso_msg_debug(image->id, "Skipping special file %s", path);
            skip = 1;
        }

        if (skip) {
            goto dir_rec_continue;
        }

        replace = image->replace;

        /* find place where to insert */
        ret = iso_dir_exists(parent, name, &pos);
        /* TODO
         * if (ret && replace == ISO_REPLACE_ASK) {
         *    replace = /....
         * }
         */
        
        /* chek if we must insert or not */
        /* TODO check for other replace behavior */
        if (ret && (replace == ISO_REPLACE_NEVER)) { 
            /* skip file */
            goto dir_rec_continue;
        }
        
        /* if we are here we must insert. Give user a chance for cancel */
        if (image->report) {
            int r = image->report(image, file);
            if (r <= 0) {
                ret = (r < 0 ? ISO_CANCELED : ISO_SUCCESS);
                goto dir_rec_continue;
            }
        }
        ret = builder->create_node(builder, image, file, &new);
        if (ret < 0) {
            ret = iso_msg_submit(image->id, ISO_FILE_CANT_ADD, ret,
                         "Error when adding file %s", path);
            goto dir_rec_continue;
        }

        /* ok, node has correctly created, we need to add it */
        ret = iso_dir_insert(parent, new, pos, replace);
        if (ret < 0) {
            iso_node_unref(new);
            if (ret != ISO_NODE_NAME_NOT_UNIQUE) {
                /* error */
                goto dir_rec_continue;
            } else {
                /* file ignored because a file with same node already exists */
                iso_msg_debug(image->id, "Skipping file %s. A node with same "
                              "file already exists", path);
                ret = 0;
            }
        } else {
            iso_msg_debug(image->id, "Added file %s", path);
        }

        /* finally, if the node is a directory we need to recurse */
        if (new->type == LIBISO_DIR && S_ISDIR(info.st_mode)) {
            ret = iso_add_dir_src_rec(image, (IsoDir*)new, file);
        }

dir_rec_continue:;
        free(path);
        iso_file_source_unref(file);
        
        /* check for error severity to decide what to do */
        if (ret < 0) {
            ret = iso_msg_submit(image->id, ret, 0, NULL);
            if (ret < 0) {
                break;
            }
        }
    } /* while */

    iso_file_source_close(dir);
    return ret < 0 ? ret : ISO_SUCCESS;
}

int iso_tree_add_dir_rec(IsoImage *image, IsoDir *parent, const char *dir)
{
    int result;
    struct stat info;
    IsoFilesystem *fs;
    IsoFileSource *file;

    if (image == NULL || parent == NULL || dir == NULL) {
        return ISO_NULL_POINTER;
    }

    fs = image->fs;
    result = fs->get_by_path(fs, dir, &file);
    if (result < 0) {
        return result;
    }

    /* we also allow dir path to be a symlink to a dir */
    result = iso_file_source_stat(file, &info);
    if (result < 0) {
        iso_file_source_unref(file);
        return result;
    }

    if (!S_ISDIR(info.st_mode)) {
        iso_file_source_unref(file);
        return ISO_FILE_IS_NOT_DIR;
    }
    result = iso_add_dir_src_rec(image, parent, file);
    iso_file_source_unref(file);
    return result;
}

int iso_tree_path_to_node(IsoImage *image, const char *path, IsoNode **node)
{
    int result;
    IsoNode *n;
    IsoDir *dir;
    char *ptr, *brk_info = NULL, *component;

    if (image == NULL || path == NULL) {
        return ISO_NULL_POINTER;
    }

    /* get the first child at the root of the image that is "/" */
    dir = image->root;
    n = (IsoNode *)dir;
    if (!strcmp(path, "/")) {
        if (node) {
            *node = n;
        }
        return ISO_SUCCESS;
    }

    ptr = strdup(path);
    result = 0;

    /* get the first component of the path */
    component = strtok_r(ptr, "/", &brk_info);
    while (component) {
        if (n->type != LIBISO_DIR) {
            n = NULL;
            result = 0;
            break;
        }
        dir = (IsoDir *)n;

        result = iso_dir_get_node(dir, component, &n);
        if (result != 1) {
            n = NULL;
            break;
        }

        component = strtok_r(NULL, "/", &brk_info);
    }

    free(ptr);
    if (node) {
        *node = n;
    }
    return result;
}

char *iso_tree_get_node_path(IsoNode *node)
{
    if (node == NULL || node->parent == NULL) {
        return NULL;
    }
    
    if ((IsoNode*)node->parent == node) {
        return strdup("/");
    } else {
        char path[PATH_MAX];
        char *parent_path = iso_tree_get_node_path((IsoNode*)node->parent);
        if (parent_path == NULL) {
            return NULL;
        }
        if (strlen(parent_path) == 1) {
            snprintf(path, PATH_MAX, "/%s", node->name);
        } else {
            snprintf(path, PATH_MAX, "%s/%s", parent_path, node->name);
        }
        free(parent_path);
        return strdup(path);
    }
}
