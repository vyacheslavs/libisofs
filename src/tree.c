/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

/*
 * Functions that act on the iso tree.
 */

#include "libisofs.h"
#include "node.h"
#include "error.h"
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
    IsoDir *node;
    IsoNode **pos;
    time_t now;

    if (parent == NULL || name == NULL) {
        return ISO_NULL_POINTER;
    }
    if (dir) {
        *dir = NULL;
    }
    
    /* check if the name is valid */
    if (!iso_node_is_valid_name(name)) {
        return ISO_WRONG_ARG_VALUE;
    }

    /* find place where to insert and check if it exists */
    if (iso_dir_exists(parent, name, &pos)) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    node = calloc(1, sizeof(IsoDir));
    if (node == NULL) {
        return ISO_MEM_ERROR;
    }

    node->node.refcount = 1;
    node->node.type = LIBISO_DIR;
    node->node.name = strdup(name);
    if (node->node.name == NULL) {
        free(node);
        return ISO_MEM_ERROR;
    }

    /* permissions from parent */
    node->node.mode = parent->node.mode;
    node->node.uid = parent->node.uid;
    node->node.gid = parent->node.gid;
    node->node.hidden = parent->node.hidden;

    /* current time */
    now = time(NULL);
    node->node.atime = now;
    node->node.ctime = now;
    node->node.mtime = now;

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
 *         ISO_MEM_ERROR
 */
int iso_tree_add_new_symlink(IsoDir *parent, const char *name,
                             const char *dest, IsoSymlink **link)
{
    IsoSymlink *node;
    IsoNode **pos;
    time_t now;

    if (parent == NULL || name == NULL || dest == NULL) {
        return ISO_NULL_POINTER;
    }
    if (link) {
        *link = NULL;
    }
    
    /* check if the name is valid */
    if (!iso_node_is_valid_name(name)) {
        return ISO_WRONG_ARG_VALUE;
    }
    
    /* check if destination is valid */
    if (!iso_node_is_valid_link_dest(dest)) {
        /* guard against null or empty dest */
        return ISO_WRONG_ARG_VALUE;
    }

    /* find place where to insert */
    if (iso_dir_exists(parent, name, &pos)) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    node = calloc(1, sizeof(IsoSymlink));
    if (node == NULL) {
        return ISO_MEM_ERROR;
    }

    node->node.refcount = 1;
    node->node.type = LIBISO_SYMLINK;
    node->node.name = strdup(name);
    if (node->node.name == NULL) {
        free(node);
        return ISO_MEM_ERROR;
    }

    node->dest = strdup(dest);
    if (node->dest == NULL) {
        free(node->node.name);
        free(node);
        return ISO_MEM_ERROR;
    }

    /* permissions from parent */
    node->node.mode = S_IFLNK | 0777;
    node->node.uid = parent->node.uid;
    node->node.gid = parent->node.gid;
    node->node.hidden = parent->node.hidden;

    /* current time */
    now = time(NULL);
    node->node.atime = now;
    node->node.ctime = now;
    node->node.mtime = now;

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
 *         ISO_MEM_ERROR
 * 
 */
int iso_tree_add_new_special(IsoDir *parent, const char *name, mode_t mode,
                             dev_t dev, IsoSpecial **special)
{
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
    
    /* check if the name is valid */
    if (!iso_node_is_valid_name(name)) {
        return ISO_WRONG_ARG_VALUE;
    }

    /* find place where to insert */
    if (iso_dir_exists(parent, name, &pos)) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    node = calloc(1, sizeof(IsoSpecial));
    if (node == NULL) {
        return ISO_MEM_ERROR;
    }

    node->node.refcount = 1;
    node->node.type = LIBISO_SPECIAL;
    node->node.name = strdup(name);
    if (node->node.name == NULL) {
        free(node);
        return ISO_MEM_ERROR;
    }

    node->node.mode = mode;
    node->dev = dev;

    /* atts from parent */
    node->node.uid = parent->node.uid;
    node->node.gid = parent->node.gid;
    node->node.hidden = parent->node.hidden;

    /* current time */
    now = time(NULL);
    node->node.atime = now;
    node->node.ctime = now;
    node->node.mtime = now;

    if (special) {
        *special = node;
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
    image->recOpts.follow_symlinks = follow ? 1 : 0;
}

/**
 * Get current setting for follow_symlinks.
 * 
 * @see iso_tree_set_follow_symlinks
 */
int iso_tree_get_follow_symlinks(IsoImage *image)
{
    return image->recOpts.follow_symlinks;
}

/**
 * Set whether to skip or not hidden files when adding a directory recursibely.
 * Default behavior is to not ignore them, i.e., to add hidden files to image.
 */
void iso_tree_set_ignore_hidden(IsoImage *image, int skip)
{
    image->recOpts.ignore_hidden = skip ? 1 : 0;
}

/**
 * Get current setting for ignore_hidden.
 * 
 * @see iso_tree_set_ignore_hidden
 */
int iso_tree_get_ignore_hidden(IsoImage *image)
{
    return image->recOpts.ignore_hidden;
}

/**
 * Set whether to stop or not when an error happens when adding recursively a 
 * directory to the iso tree. Default value is to skip file and continue.
 */
void iso_tree_set_stop_on_error(IsoImage *image, int stop)
{
    image->recOpts.stop_on_error = stop ? 1 : 0;
}

/**
 * Get current setting for stop_on_error.
 * 
 * @see iso_tree_set_stop_on_error
 */
int iso_tree_get_stop_on_error(IsoImage *image)
{
    return image->recOpts.stop_on_error;
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

static
int check_excludes(IsoImage *image, const char *path)
{
    char **exclude;
    if (image->recOpts.excludes == NULL) {
        return 0;
    }
    exclude = image->recOpts.excludes;
    while (*exclude) {
        if (strcmp(*exclude, path) == 0) {
            return 1;
        }
        ++exclude;
    }
    return 0;
}

static
int check_hidden(IsoImage *image, const char *name)
{
    return (image->recOpts.ignore_hidden && name[0] == '.');
}

/**
 * Recursively add a given directory to the image tree.
 * 
 * @return
 *      1 continue, 0 stop, < 0 error
 */
int iso_add_dir_src_rec(IsoImage *image, IsoDir *parent, IsoFileSource *dir)
{
    int result;
    int action; /* 1 add, 2 skip, 3 stop, < 0 error */
    IsoNodeBuilder *builder;
    IsoFileSource *file;
    IsoNode **pos;

    result = iso_file_source_open(dir);
    if (result < 0) {
        char *path = iso_file_source_get_path(dir);
        iso_msg_debug(image->messenger, "Can't open dir %s", path);
        free(path);
        return result;
    }

    builder = image->builder;
    action = 1;
    while ( (result = iso_file_source_readdir(dir, &file)) == 1) {
        int flag;
        char *name, *path;
        IsoNode *new;

        name = iso_file_source_get_name(file);
        path = iso_file_source_get_path(file);

        if (check_excludes(image, path)) {
            iso_msg_debug(image->messenger, "Skipping excluded file %s", path);
            action = 2;
        } else if (check_hidden(image, name)) {
            iso_msg_debug(image->messenger, "Skipping hidden file %s", path);
            action = 2;
        } else {
            iso_msg_debug(image->messenger, "Adding file %s", path);
            action = 1;
        }
        free(path);

        /* find place where to insert */
        flag = 0;
        if (iso_dir_exists(parent, name, &pos)) {
            flag = 1;
            if (action == 1 && image->recOpts.replace == ISO_REPLACE_NEVER) {
                action = 2;
            }
        }

        /* name no more needed */
        free(name);

        /* ask user if callback has been set */
        if (image->recOpts.report) {
            action = image->recOpts.report(file, action, flag);
        }

        if (action == 2) {
            /* skip file */
            iso_file_source_unref(file);
            continue;
        } else if (action == 3) {
            /* stop */
            iso_file_source_unref(file);
            break;
        }

        /* ok, file will be added */
        result = builder->create_node(builder, image, file, &new);
        if (result < 0) {
            char *path = iso_file_source_get_path(file);
            iso_msg_note(image->messenger, LIBISO_FILE_IGNORED, 
                         "Error %d when adding file %s", result, path);
            free(path);

            if (image->recOpts.report) {
                action = image->recOpts.report(file, result, flag);
            } else {
                action = image->recOpts.stop_on_error ? 3 : 1;
            }

            /* free file */
            iso_file_source_unref(file);

            if (action == 3) {
                result = 1; /* prevent error to be passing up */
                break;
            } else {
                /* TODO check that action is 1!!! */
                continue;
            }
        }

        /* ok, node has correctly created, we need to add it */
        iso_dir_insert(parent, new, pos, flag ? ISO_REPLACE_ALWAYS :
                                         ISO_REPLACE_NEVER);

        /* finally, if the node is a directory we need to recurse */
        if (new->type == LIBISO_DIR) {
            result = iso_add_dir_src_rec(image, (IsoDir*)new, file);
            iso_file_source_unref(file);
            if (result < 0) {
                /* error */
                if (image->recOpts.stop_on_error) {
                    action = 3; /* stop */
                    result = 1; /* prevent error to be passing up */
                    break;
                }
            } else if (result == 0) {
                /* stop */
                action = 3;
                break;
            }
        } else {
            iso_file_source_unref(file);
        }
    }

    if (result < 0) {
        /* error reading dir, should never occur */
        iso_msg_sorry(image->messenger, LIBISO_CANT_READ_FILE, 
                      "Error reading dir");
        action = result;
    }

    result = iso_file_source_close(dir);
    if (result < 0) {
        return result;
    }
    if (action < 0) {
        return action; /* error */
    } else if (action == 3) {
        return 0; /* stop */
    } else {
        return 1; /* continue */
    }
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
    char *ptr, *brk_info, *component;

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
