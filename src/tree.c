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
    
    /* find place where to insert */
    pos = &(parent->children);
    while (*pos != NULL && strcmp((*pos)->name, name) < 0) {
        pos = &((*pos)->next);
    }
    if (*pos != NULL && !strcmp((*pos)->name, name)) {
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
    
    /* add to dir */
    node->node.parent = parent;
    node->node.next = *pos;
    *pos = (IsoNode*)node;
    
    if (dir) {
        *dir = node;
    }
    return ++parent->nchildren;
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
    
    /* find place where to insert */
    pos = &(parent->children);
    while (*pos != NULL && strcmp((*pos)->name, name) < 0) {
        pos = &((*pos)->next);
    }
    if (*pos != NULL && !strcmp((*pos)->name, name)) {
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
    
    /* add to dir */
    node->node.parent = parent;
    node->node.next = *pos;
    *pos = (IsoNode*)node;
    
    if (link) {
        *link = node;
    }
    return ++parent->nchildren;
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
    
    /* find place where to insert */
    pos = &(parent->children);
    while (*pos != NULL && strcmp((*pos)->name, name) < 0) {
        pos = &((*pos)->next);
    }
    if (*pos != NULL && !strcmp((*pos)->name, name)) {
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
    
    /* add to dir */
    node->node.parent = parent;
    node->node.next = *pos;
    *pos = (IsoNode*)node;
    
    if (special) {
        *special = node;
    }
    return ++parent->nchildren;
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
    
    name = src->get_name(src);
    
    /* find place where to insert */
    pos = &(parent->children);
    while (*pos != NULL && strcmp((*pos)->name, name) < 0) {
        pos = &((*pos)->next);
    }
    if (*pos != NULL && !strcmp((*pos)->name, name)) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }
    
    result = builder->create_node(builder, image, src, &new);
    if (result < 0) {
        return result;
    }
    
    /* finally, add node to parent */
    new->parent = parent;
    new->next = *pos;
    *pos = new;
    
    if (node) {
        *node = new;
    }
    return ++parent->nchildren;
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
    if (image->recOpts->excludes == NULL) {
        return 0;
    }
    exclude = image->recOpts->excludes;
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
    return (image->recOpts->ignore_hidden && name[0] == '.');
}

/**
 * @return
 *      1 continue, 0 stop, < 0 error
 */
static
int iso_add_dir_aux(IsoImage *image, IsoDir *parent, IsoFileSource *dir)
{
    int result;
    int action; /* 1 add, 2 skip, 3 stop, < 0 error */
    IsoNodeBuilder *builder;
    IsoFileSource *file;
    IsoNode **pos;
    
    result = dir->open(dir);
    if (result < 0) {
        iso_msg_debug(image, "Can't open dir %s", dir->get_path(dir));
        return result;
    }
    
    builder = image->builder;
    action = 1;
    while ( (result = dir->readdir(dir, &file)) == 1) {
        int flag;
        char *name;
        IsoNode *new;
        
        iso_msg_debug(image, "Adding file %s", file->get_path(file));
        
        name = file->get_name(file);
        
        if (check_excludes(image, file->get_path(file))) {
            action = 2;
        } else if (check_hidden(image, name)) {
            action = 2;
        } else {
            action = 1;
        }
        
        /* find place where to insert */
        flag = 0;
        pos = &(parent->children);
        while (*pos != NULL && strcmp((*pos)->name, name) < 0) {
            pos = &((*pos)->next);
        }
        if (*pos != NULL && !strcmp((*pos)->name, name)) {
            flag = 1;
            if (action == 1 && image->recOpts->replace == 0) {
                action = 2;
            }
        }
        
        /* ask user if callback has been set */
        if (image->recOpts->report) {
            action = image->recOpts->report(file, action, flag);
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
            
            iso_msg_debug(image, "Error %d when adding file %s", 
                          result, file->get_path(file));
            
            if (image->recOpts->report) {
                action = image->recOpts->report(file, result, flag);
            } else {
                action = image->recOpts->stop_on_error ? 3 : 1;
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
        if (flag) {
            /* replace node */
            new->next = (*pos)->next;
            (*pos)->parent = NULL;
            (*pos)->next = NULL;
            iso_node_unref(*pos);
            *pos = new;
            new->parent = parent;
        } else {
            /* just add */
            new->next = *pos;
            *pos = new;
            new->parent = parent;
            ++parent->nchildren;
        }
        
        /* finally, if the node is a directory we need to recurse */
        if (new->type == LIBISO_DIR) {
            result = iso_add_dir_aux(image, (IsoDir*)new, file);
            iso_file_source_unref(file);
            if (result < 0) {
                /* error */
                if (image->recOpts->stop_on_error) {
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
        // TODO printf message
        action = result;
    }
    
    result = dir->close(dir);
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
    result = file->stat(file, &info);
    if (result < 0) {
        iso_file_source_unref(file);
        return result;
    }
    
    if (!S_ISDIR(info.st_mode)) {
        iso_file_source_unref(file);
        return ISO_FILE_IS_NOT_DIR;
    }
    result = iso_add_dir_aux(image, parent, file);
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
