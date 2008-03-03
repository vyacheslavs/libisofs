/*
 * Copyright (c) 2008 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "libisofs.h"
#include "node.h"

#include <fnmatch.h>
#include <string.h>

struct iso_find_condition
{
    /*
     * Check whether the given node matches this condition.
     * 
     * @param cond
     *      The condition to check
     * @param node
     *      The node that should be checked
     * @return
     *      1 if the node matches the condition, 0 if not
     */
    int (*matches)(IsoFindCondition *cond, IsoNode *node);
    
    /**
     * Free condition specific data
     */
    void (*free)(IsoFindCondition*);
    
    /** condition specific data */
    void *data;
};

struct find_iter_data
{
    IsoDirIter *iter;
    IsoFindCondition *cond;
};

static
int find_iter_next(IsoDirIter *iter, IsoNode **node)
{
    int ret;
    IsoNode *n;
    struct find_iter_data *data = iter->data;
    
    while ((ret = iso_dir_iter_next(data->iter, &n)) == 1) {
        if (data->cond->matches(data->cond, n)) {
            *node = n;
            break;
        }
    }
    return ret;
}

static
int find_iter_has_next(IsoDirIter *iter)
{
    struct find_iter_data *data = iter->data;
    
    /*
     * FIXME wrong implementation!!!! the underlying iter may have more nodes,
     * but they may not match find conditions
     */
    return iso_dir_iter_has_next(data->iter);
}

static
void find_iter_free(IsoDirIter *iter)
{
    struct find_iter_data *data = iter->data;
    data->cond->free(data->cond);
    free(data->cond);
    iso_dir_iter_free(data->iter);
    free(iter->data);
}

static
int find_iter_take(IsoDirIter *iter)
{
    struct find_iter_data *data = iter->data;
    return iso_dir_iter_take(data->iter);
}

static
int find_iter_remove(IsoDirIter *iter)
{
    struct find_iter_data *data = iter->data;
    return iso_dir_iter_remove(data->iter);
}

static
struct iso_dir_iter_iface find_iter_class = {
        find_iter_next,
        find_iter_has_next,
        find_iter_free,
        find_iter_take,
        find_iter_remove
};

int iso_dir_find_children(IsoDir* dir, IsoFindCondition *cond, 
                          IsoDirIter **iter)
{
    int ret;
    IsoDirIter *children;
    IsoDirIter *it;
    struct find_iter_data *data;

    if (dir == NULL || cond == NULL || iter == NULL) {
        return ISO_NULL_POINTER;
    }
    it = malloc(sizeof(IsoDirIter));
    if (it == NULL) {
        return ISO_OUT_OF_MEM;
    }
    data = malloc(sizeof(struct find_iter_data));
    if (data == NULL) {
        free(it);
        return ISO_OUT_OF_MEM;
    }
    ret = iso_dir_get_children(dir, &children);
    if (ret < 0) {
        free(it);
        free(data);
        return ret;
    }

    it->class = &find_iter_class;
    it->dir = (IsoDir*)dir;
    data->iter = children;
    data->cond = cond;
    it->data = data;

    *iter = it;
    return ISO_SUCCESS;
}

/*************** find by name wildcard condition *****************/

static
int cond_name_matches(IsoFindCondition *cond, IsoNode *node)
{
    char *pattern = (char*) cond->data;
    int ret = fnmatch(pattern, node->name, 0);
    return ret == 0 ? 1 : 0;
}

static
void cond_name_free(IsoFindCondition *cond)
{
    free(cond->data);
}

/**
 * Create a new condition that checks if the node name matches the given
 * wildcard.
 * 
 * @param wildcard
 * @result
 *      The created IsoFindCondition, NULL on error.
 * 
 * @since 0.6.4
 */
IsoFindCondition *iso_new_find_conditions_name(const char *wildcard)
{
    IsoFindCondition *cond;
    if (wildcard == NULL) {
        return NULL;
    }
    cond = malloc(sizeof(IsoFindCondition));
    if (cond == NULL) {
        return NULL;
    }
    cond->data = strdup(wildcard);
    cond->free = cond_name_free;
    cond->matches = cond_name_matches;
    return cond;
}

/*************** find by mode condition *****************/

static
int cond_mode_matches(IsoFindCondition *cond, IsoNode *node)
{
    mode_t *mask = (mode_t*) cond->data;
    return node->mode & *mask ? 1 : 0;
}

static
void cond_mode_free(IsoFindCondition *cond)
{
    free(cond->data);
}

/**
 * Create a new condition that checks the node mode against a mode mask. It
 * can be used to check both file type and permissions.
 * 
 * For example:
 * 
 * iso_new_find_conditions_mode(S_IFREG) : search for regular files
 * iso_new_find_conditions_mode(S_IFCHR | S_IWUSR) : search for character 
 *     devices where owner has write permissions.
 * 
 * @param mask
 *      Mode mask to AND against node mode.
 * @result
 *      The created IsoFindCondition, NULL on error.
 * 
 * @since 0.6.4
 */
IsoFindCondition *iso_new_find_conditions_mode(mode_t mask)
{
    IsoFindCondition *cond;
    mode_t *data;
    cond = malloc(sizeof(IsoFindCondition));
    if (cond == NULL) {
        return NULL;
    }
    data = malloc(sizeof(mode_t));
    if (data == NULL) {
        free(cond);
        return NULL;
    }
    *data = mask;
    cond->data = data;
    cond->free = cond_mode_free;
    cond->matches = cond_mode_matches;
    return cond;
}

/*************** find by gid condition *****************/

static
int cond_gid_matches(IsoFindCondition *cond, IsoNode *node)
{
    gid_t *gid = (gid_t*) cond->data;
    return node->gid == *gid ? 1 : 0;
}

static
void cond_gid_free(IsoFindCondition *cond)
{
    free(cond->data);
}

/**
 * Create a new condition that checks the node gid.
 * 
 * @param gid
 *      Desired Group Id.
 * @result
 *      The created IsoFindCondition, NULL on error.
 * 
 * @since 0.6.4
 */
IsoFindCondition *iso_new_find_conditions_gid(gid_t gid)
{
    IsoFindCondition *cond;
    gid_t *data;
    cond = malloc(sizeof(IsoFindCondition));
    if (cond == NULL) {
        return NULL;
    }
    data = malloc(sizeof(gid_t));
    if (data == NULL) {
        free(cond);
        return NULL;
    }
    *data = gid;
    cond->data = data;
    cond->free = cond_gid_free;
    cond->matches = cond_gid_matches;
    return cond;
}

/*************** find by uid condition *****************/

static
int cond_uid_matches(IsoFindCondition *cond, IsoNode *node)
{
    uid_t *uid = (uid_t*) cond->data;
    return node->uid == *uid ? 1 : 0;
}

static
void cond_uid_free(IsoFindCondition *cond)
{
    free(cond->data);
}

/**
 * Create a new condition that checks the node uid.
 * 
 * @param uid
 *      Desired User Id.
 * @result
 *      The created IsoFindCondition, NULL on error.
 * 
 * @since 0.6.4
 */
IsoFindCondition *iso_new_find_conditions_uid(uid_t uid)
{
    IsoFindCondition *cond;
    uid_t *data;
    cond = malloc(sizeof(IsoFindCondition));
    if (cond == NULL) {
        return NULL;
    }
    data = malloc(sizeof(uid_t));
    if (data == NULL) {
        free(cond);
        return NULL;
    }
    *data = uid;
    cond->data = data;
    cond->free = cond_uid_free;
    cond->matches = cond_uid_matches;
    return cond;
}

