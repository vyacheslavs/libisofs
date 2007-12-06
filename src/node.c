/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "libisofs.h"
#include "node.h"
#include "error.h"
#include "stream.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Increments the reference counting of the given node.
 */
void iso_node_ref(IsoNode *node)
{
    ++node->refcount;
}

/**
 * Decrements the reference couting of the given node.
 * If it reach 0, the node is free, and, if the node is a directory,
 * its children will be unref() too.
 */
void iso_node_unref(IsoNode *node)
{
    if (--node->refcount == 0) {
        switch(node->type) {
        case LIBISO_DIR:
            {
                IsoNode *child = ((IsoDir*)node)->children;
                while (child != NULL) {
                    IsoNode *tmp = child->next;
                    child->parent = NULL;
                    iso_node_unref(child);
                    child = tmp;
                }
            }
            break;
        case LIBISO_FILE:
            {
                IsoFile *file = (IsoFile*) node;
                iso_stream_unref(file->stream);
            }
            break;
        case LIBISO_SYMLINK:
            {
                IsoSymlink *link = (IsoSymlink*) node;
                free(link->dest);
            }
        default:
            /* TODO #00002 handle deletion of each kind of node */
            break;    
        }
        free(node->name);
        free(node);
    }
}

/**
 * Set the name of a node.
 * 
 * @param name  The name in UTF-8 encoding
 */
int iso_node_set_name(IsoNode *node, const char *name)
{
    char *new;
    if (node->parent != NULL) {
        /* check if parent already has a node with same name */
        if (iso_dir_get_node(node->parent, name, NULL) == 1) {
            return ISO_NODE_NAME_NOT_UNIQUE;
        }
    }
    new = strdup(name);
    if (new == NULL) {
        return ISO_MEM_ERROR;
    }
    free(node->name);
    node->name = new;
    if (node->parent != NULL) {
        IsoDir *parent;
        int res;
        /* take and add again to ensure correct children order */
        parent = node->parent;
        iso_node_take(node);
        res = iso_dir_add_node(parent, node, 0);
        if (res < 0) {
            return res;
        }
    }
    return ISO_SUCCESS; 
}

/**
 * Get the name of a node (in UTF-8).
 * The returned string belongs to the node and should not be modified nor
 * freed. Use strdup if you really need your own copy.
 */
const char *iso_node_get_name(const IsoNode *node)
{
    return node->name;
}

/**
 * Set the permissions for the node. This attribute is only useful when 
 * Rock Ridge extensions are enabled.
 * 
 * @param mode 
 *     bitmask with the permissions of the node, as specified in 'man 2 stat'.
 *     The file type bitfields will be ignored, only file permissions will be
 *     modified.
 */
void iso_node_set_permissions(IsoNode *node, mode_t mode)
{
    node->mode = (node->mode & S_IFMT) | (mode & ~S_IFMT);
}

/** 
 * Get the permissions for the node 
 */
mode_t iso_node_get_permissions(const IsoNode *node)
{
    return node->mode & ~S_IFMT;
}

/** 
 * Get the mode of the node, both permissions and file type, as specified in
 * 'man 2 stat'.
 */
mode_t iso_node_get_mode(const IsoNode *node)
{
    return node->mode;
}


/**
 * Set the user id for the node. This attribute is only useful when 
 * Rock Ridge extensions are enabled.
 */
void iso_node_set_uid(IsoNode *node, uid_t uid)
{
    node->uid = uid;
}

/**
 * Get the user id of the node.
 */
uid_t iso_node_get_uid(const IsoNode *node)
{
    return node->uid;
}

/**
 * Set the group id for the node. This attribute is only useful when 
 * Rock Ridge extensions are enabled.
 */
void iso_node_set_gid(IsoNode *node, gid_t gid)
{
    node->gid = gid;
}

/**
 * Get the group id of the node.
 */
gid_t iso_node_get_gid(const IsoNode *node)
{
    return node->gid;
}

/** 
 * Set the time of last modification of the file
 */
void iso_node_set_mtime(IsoNode *node, time_t time)
{
    node->mtime = time;
}

/** 
 * Get the time of last modification of the file
 */
time_t iso_node_get_mtime(const IsoNode *node)
{
    return node->mtime;
}

/** 
 * Set the time of last access to the file
 */
void iso_node_set_atime(IsoNode *node, time_t time)
{
    node->atime = time;
}

/** 
 * Get the time of last access to the file 
 */
time_t iso_node_get_atime(const IsoNode *node)
{
    return node->atime;
}

/** 
 * Set the time of last status change of the file 
 */
void iso_node_set_ctime(IsoNode *node, time_t time)
{
    node->ctime = time;
}

/** 
 * Get the time of last status change of the file 
 */
time_t iso_node_get_ctime(const IsoNode *node)
{
    return node->ctime;
}

/**
 * Add a new node to a dir. Note that this function don't add a new ref to
 * the node, so you don't need to free it, it will be automatically freed
 * when the dir is deleted. Of course, if you want to keep using the node
 * after the dir life, you need to iso_node_ref() it.
 * 
 * @param dir 
 *     the dir where to add the node
 * @param child 
 *     the node to add. You must ensure that the node hasn't previously added
 *     to other dir, and that the node name is unique inside the child.
 *     Otherwise this function will return a failure, and the child won't be
 *     inserted.
 * @return
 *     number of nodes in dir if succes, < 0 otherwise
 */
int iso_dir_add_node(IsoDir *dir, IsoNode *child, int replace)
{
    IsoNode **pos;
    
    if (dir == NULL || child == NULL) {
        return ISO_NULL_POINTER;
    }
    if ((IsoNode*)dir == child) {
        return ISO_WRONG_ARG_VALUE;
    }
    
    /* 
     * check if child is already added to another dir, or if child
     * is the root node, where parent == itself
     */
    if (child->parent != NULL || child->parent == (IsoDir*)child) {
        return ISO_NODE_ALREADY_ADDED;
    }
    
    pos = &(dir->children);
    while (*pos != NULL && strcmp((*pos)->name, child->name) < 0) {
        pos = &((*pos)->next);
    }
    if (*pos != NULL && !strcmp((*pos)->name, child->name)) {
        /* a node with same name already exists */
        if (replace == 0) {
            return ISO_NODE_NAME_NOT_UNIQUE;
        } else if (replace == 1) {
            child->next = (*pos)->next;
            (*pos)->parent = NULL;
            (*pos)->next = NULL;
            iso_node_unref(*pos);
            *pos = child;
            child->parent = dir;
            return dir->nchildren;
        } else {
            return ISO_WRONG_ARG_VALUE;
        }
    }
    
    child->next = *pos;
    *pos = child;
    child->parent = dir;
    
    return ++dir->nchildren;
}

/**
 * Locate a node inside a given dir.
 * 
 * @param name
 *     The name of the node
 * @param node
 *     Location for a pointer to the node, it will filled with NULL if the dir 
 *     doesn't have a child with the given name.
 *     The node will be owned by the dir and shouldn't be unref(). Just call
 *     iso_node_ref() to get your own reference to the node.
 *     Note that you can pass NULL is the only thing you want to do is check
 *     if a node with such name already exists on dir.
 * @return 
 *     1 node found, 0 child has no such node, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if dir or name are NULL
 */
int iso_dir_get_node(IsoDir *dir, const char *name, IsoNode **node)
{
    IsoNode *pos;
    if (dir == NULL || name == NULL) {
        return ISO_NULL_POINTER;
    }
    
    pos = dir->children;
    while (pos != NULL && strcmp(pos->name, name) < 0) {
        pos = pos->next;
    }
    
    if (pos == NULL || strcmp(pos->name, name)) {
        if (node) {
            *node = NULL;
        }
        return 0; /* node not found */
    }
    
    if (node) {
        *node = pos;
    }
    return 1;
}

/**
 * Get the number of children of a directory.
 * 
 * @return
 *     >= 0 number of items, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if dir is NULL
 */
int iso_dir_get_nchildren(IsoDir *dir)
{
    if (dir == NULL) {
        return ISO_NULL_POINTER;
    }
    return dir->nchildren;
}

int iso_dir_get_children(const IsoDir *dir, IsoDirIter **iter)
{
    IsoDirIter *it;
    
    if (dir == NULL || iter == NULL) {
        return ISO_NULL_POINTER;
    }
    it = malloc(sizeof(IsoDirIter));
    if (it == NULL) {
        return ISO_OUT_OF_MEM;
    }
    
    it->dir = dir;
    it->pos = dir->children;
    
    *iter = it;
    return ISO_SUCCESS;
}

int iso_dir_iter_next(IsoDirIter *iter, IsoNode **node)
{
    IsoNode *n;
    if (iter == NULL || node == NULL) {
        return ISO_NULL_POINTER;
    }
    n = iter->pos;
    if (n == NULL) {
        *node = NULL;
        return 0;
    }
    if (n->parent != iter->dir) {
        /* this can happen if the node has been moved to another dir */
        return ISO_ERROR;
    }
    *node = n;
    iter->pos = n->next;
    return ISO_SUCCESS;
}

/**
 * Check if there're more children.
 * 
 * @return
 *     1 dir has more elements, 0 no, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if iter is NULL
 */
int iso_dir_iter_has_next(IsoDirIter *iter)
{
    if (iter == NULL) {
        return ISO_NULL_POINTER;
    }
    return iter->pos == NULL ? 0 : 1;
}

void iso_dir_iter_free(IsoDirIter *iter)
{
    free(iter);
}

static IsoNode**
iso_dir_find_node(IsoDir *dir, IsoNode *node)
{
    IsoNode **pos;
    pos = &(dir->children);
    while (*pos != NULL && *pos != node) {
        pos = &((*pos)->next);
    } 
    return pos;
}

/**
 * Removes a child from a directory.
 * The child is not freed, so you will become the owner of the node. Later
 * you can add the node to another dir (calling iso_dir_add_node), or free
 * it if you don't need it (with iso_node_unref).
 * 
 * @return 
 *     1 on success, < 0 error
 */
int iso_node_take(IsoNode *node)
{
    IsoNode **pos;
    IsoDir* dir;
    
    if (node == NULL) {
        return ISO_NULL_POINTER;
    }
    dir = node->parent;
    if (dir == NULL) {
        return ISO_NODE_NOT_ADDED_TO_DIR;
    }
    pos = iso_dir_find_node(dir, node);
    if (pos == NULL) {
        /* should never occur */
        return ISO_ERROR;
    }
    *pos = node->next;
    node->parent = NULL;
    node->next = NULL;
    dir->nchildren--;
    return ISO_SUCCESS;
}

/**
 * Removes a child from a directory and free (unref) it.
 * If you want to keep the child alive, you need to iso_node_ref() it
 * before this call, but in that case iso_node_take() is a better
 * alternative.
 * 
 * @return 
 *     1 on success, < 0 error
 */
int iso_node_remove(IsoNode *node)
{
    int ret;
    ret = iso_node_take(node);
    if (ret == ISO_SUCCESS) {
        iso_node_unref(node);
    }
    return ret;
}

/* TODO #00005 optimize iso_dir_iter_take */
int iso_dir_iter_take(IsoDirIter *iter)
{
    IsoNode *pos;
    if (iter == NULL) {
        return ISO_NULL_POINTER;
    }
    
    pos = iter->dir->children;
    if (iter->pos == pos) {
        return ISO_ERROR;
    }
    while (pos != NULL && pos->next == iter->pos) {
        pos = pos->next;
    }
    if (pos == NULL) {
        return ISO_ERROR;
    }
    return iso_node_take(pos);
}

int iso_dir_iter_remove(IsoDirIter *iter)
{
    IsoNode *pos;
    if (iter == NULL) {
        return ISO_NULL_POINTER;
    }
    pos = iter->dir->children;
    if (iter->pos == pos) {
        return ISO_ERROR;
    }
    while (pos != NULL && pos->next == iter->pos) {
        pos = pos->next;
    }
    if (pos == NULL) {
        return ISO_ERROR;
    }
    return iso_node_remove(pos);
}

/**
 * Get the destination of a node (in UTF-8).
 * The returned string belongs to the node and should not be modified nor
 * freed. Use strdup if you really need your own copy.
 */
const char *iso_symlink_get_dest(const IsoSymlink *link)
{
    return link->dest;
}

/**
 * Set the destination of a link.
 */
void iso_symlink_set_dest(IsoSymlink *link, const char *dest)
{
    free(link->dest);
    link->dest = strdup(dest);
}

int iso_node_new_root(IsoDir **root)
{
    IsoDir *dir;
    
    dir = calloc(1, sizeof(IsoDir));
    if (dir == NULL) {
        return ISO_MEM_ERROR;
    }
    dir->node.type = LIBISO_DIR;
    dir->node.atime = dir->node.ctime = dir->node.mtime = time(NULL);
    dir->node.mode = S_IFDIR | 0555;
    
    /* set parent to itself, to prevent root to be added to another dir */
    dir->node.parent = dir;
    *root = dir;
    return ISO_SUCCESS;
}
