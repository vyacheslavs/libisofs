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

#include <stdlib.h>
#include <string.h>

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
                    iso_node_unref(child);
                    child = tmp;
                }
            }
            break;
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
void iso_node_set_name(IsoNode *node, const char *name)
{
    free(node->name);
    node->name = strdup(name);
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
int iso_dir_add_node(IsoDir *dir, IsoNode *child)
{
    IsoNode **pos;
    
    if (dir == NULL || child == NULL) {
        return ISO_NULL_POINTER;
    }
    if ((IsoNode*)dir == child) {
        return ISO_WRONG_ARG_VALUE;
    }
    if (child->parent != NULL) {
        return ISO_NODE_ALREADY_ADDED;
    }
    
    pos = &(dir->children);
    while (*pos != NULL && strcmp((*pos)->name, child->name) < 0) {
        pos = &((*pos)->next);
    }
    if (*pos != NULL && !strcmp((*pos)->name, child->name)) {
        return ISO_NODE_NAME_NOT_UNIQUE;
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
 * @return 
 *     1 node found, 0 child has no such node, < 0 error
 */
int iso_dir_get_node(IsoDir *dir, const char *name, IsoNode **node)
{
    IsoNode *pos;
    if (dir == NULL || name == NULL || node == NULL) {
        return ISO_NULL_POINTER;
    }
    
    pos = dir->children;
    while (pos != NULL && strcmp(pos->name, name) < 0) {
        pos = pos->next;
    }
    
    if (pos == NULL || strcmp(pos->name, name)) {
        *node = NULL;
        return 0; /* node not found */
    }
    
    *node = pos;
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
