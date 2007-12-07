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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

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
int iso_tree_add_node_builder(IsoDir *parent, IsoFileSource *src,
                              IsoNodeBuilder *builder, IsoNode **node)
{
    int result;
    struct stat info;
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
    
    /* get info about source */
    result = src->lstat(src, &info);
    if (result < 0) {
        return result;
    }
    
    new = NULL;
    switch (info.st_mode & S_IFMT) {
    case S_IFREG:
        {
            /* source is a regular file */
            IsoStream *stream;
            IsoFile *file;
            result = iso_file_source_stream_new(src, &stream);
            if (result < 0) {
                return result;
            }
            file = malloc(sizeof(IsoFile));
            if (file == NULL) {
                iso_stream_unref(stream);
                return ISO_MEM_ERROR;
            }
            file->msblock = 0;
            file->sort_weight = 0;
            file->stream = stream;
            file->node.type = LIBISO_FILE;
            new = (IsoNode*) file;
        }
        break;
    case S_IFDIR:
        {
            /* source is a directory */
            new = calloc(1, sizeof(IsoDir));
            if (new == NULL) {
                return ISO_MEM_ERROR;
            }
            new->type = LIBISO_DIR;
        }
        break;
    case S_IFLNK:
        {
            /* source is a symbolic link */
            char dest[PATH_MAX];
            IsoSymlink *link;
            
            result = src->readlink(src, dest, PATH_MAX);
            if (result < 0) {
                return result;
            }
            link = malloc(sizeof(IsoSymlink));
            if (link == NULL) {
                return ISO_MEM_ERROR;
            }
            link->dest = strdup(dest);
            link->node.type = LIBISO_SYMLINK;
            new = (IsoNode*) link;
        }
        break;
    case S_IFSOCK:
    case S_IFBLK:
    case S_IFCHR:
    case S_IFIFO:
        {
            /* source is an special file */
            IsoSpecial *special;
            special = malloc(sizeof(IsoSpecial));
            if (special == NULL) {
                return ISO_MEM_ERROR;
            }
            special->dev = info.st_rdev;
            special->node.type = LIBISO_SPECIAL;
            new = (IsoNode*) special;
        }
        break;
    }
    
    /* fill fields */
    new->refcount = 1;
    new->name = strdup(name);
    new->mode = info.st_mode;
    new->uid = info.st_uid;
    new->gid = info.st_gid;
    new->atime = info.st_atime;
    new->mtime = info.st_mtime;
    new->ctime = info.st_ctime;
    
    new->hidden = 0;
    
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
    result = iso_tree_add_node_builder(parent, file, image->builder, node);
    return result;
}
