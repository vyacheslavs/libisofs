/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "ecma119_tree.h"
#include "ecma119.h"
#include "error.h"
#include "node.h"
#include "util.h"
#include "filesrc.h"

#include <stdlib.h>
#include <string.h>

static
int set_iso_name(Ecma119Image *img, IsoNode *iso, Ecma119Node *node)
{
    int ret;
    char *ascii_name;

    if (iso->name == NULL) {
        /* it is not necessarily an error, it can be the root */
        return ISO_SUCCESS;
    }

    // TODO add support for other input charset
    ret = str2ascii("UTF-8", iso->name, &ascii_name);
    if (ret < 0) {
        return ret;
    }

    // TODO add support for relaxed constraints
    if (iso->type == LIBISO_DIR) {
        if (img->iso_level == 1) {
            iso_dirid(ascii_name, 8);
        } else {
            iso_dirid(ascii_name, 31);
        }
    } else {
        if (img->iso_level == 1) {
            iso_1_fileid(ascii_name);
        } else {
            iso_2_fileid(ascii_name);
        }
    }
    node->iso_name = ascii_name;
    return ISO_SUCCESS;
}

static
int create_ecma119_node(Ecma119Image *img, IsoNode *iso, Ecma119Node **node)
{
    int ret;
    Ecma119Node *ecma;

    ecma = calloc(1, sizeof(Ecma119Node));
    if (ecma == NULL) {
        return ISO_MEM_ERROR;
    }

    ret = set_iso_name(img, iso, ecma);
    if (ret < 0) {
        free(ecma);
        return ret;
    }

    /* take a ref to the IsoNode */
    ecma->node = iso;
    iso_node_ref(iso);

    // TODO what to do with full name? For now, not a problem, as we
    // haven't support for charset conversion. However, one we had it,
    // we need to choose whether to do it here (consumes more memory)
    // or on writting
    *node = ecma;
    return ISO_SUCCESS;
}

/**
 * Create a new ECMA-119 node representing a directory from a iso directory
 * node.
 */
static
int create_dir(Ecma119Image *img, IsoDir *iso, Ecma119Node **node)
{
    int ret;
    Ecma119Node **children;

    children = calloc(1, sizeof(void*) * iso->nchildren);
    if (children == NULL) {
        return ISO_MEM_ERROR;
    }

    ret = create_ecma119_node(img, (IsoNode*)iso, node);
    if (ret < 0) {
        free(children);
        return ret;
    }
    (*node)->type = ECMA119_DIR;
    (*node)->info.dir.nchildren = 0;
    (*node)->info.dir.children = children;
    return ISO_SUCCESS;
}

/**
 * Create a new ECMA-119 node representing a regular file from a iso file
 * node.
 */
static
int create_file(Ecma119Image *img, IsoFile *iso, Ecma119Node **node)
{
    int ret;
    IsoFileSrc *src;

    ret = iso_file_src_create(img, iso, &src);
    if (ret < 0) {
        return ret;
    }

    ret = create_ecma119_node(img, (IsoNode*)iso, node);
    if (ret < 0) {
        /* 
         * the src doesn't need to be freed, it is free together with
         * the Ecma119Image 
         */
        return ret;
    }
    (*node)->type = ECMA119_FILE;
    (*node)->info.file = src;
    
    return ret;
}

void ecma119_node_free(Ecma119Node *node)
{
    if (node->type == ECMA119_DIR) {
        int i;
        for (i = 0; i < node->info.dir.nchildren; i++) {
            ecma119_node_free(node->info.dir.children[i]);
        }
        free(node->info.dir.children);
    }
    free(node->iso_name);
    iso_node_unref(node->node);
    //TODO? free(node->name);
    free(node);
}

/**
 * 
 * @return 
 *      1 success, 0 node ignored,  < 0 error
 * 
 */
static
int create_tree(Ecma119Image *image, IsoNode *iso, Ecma119Node **tree)
{
    int ret;
    Ecma119Node *node;

    if (image == NULL || iso == NULL || tree == NULL) {
        return ISO_NULL_POINTER;
    }

    if (iso->hidden & LIBISO_HIDE_ON_RR) {
        /* file will be ignored */
        return 0;
    }

    switch(iso->type) {
    case LIBISO_FILE:
        ret = create_file(image, (IsoFile*)iso, &node);
        break;
    case LIBISO_SYMLINK:
        //TODO only supported with RR
        return 0;
        break;
    case LIBISO_SPECIAL:
        //TODO only supported with RR
        return 0;
        break;
    case LIBISO_BOOT:
        //TODO
        return 0;
        break;
    case LIBISO_DIR: 
        {
            IsoNode *pos;
            IsoDir *dir = (IsoDir*)iso;
            ret = create_dir(image, dir, &node);
            if (ret < 0) {
                return ret;
            }
            pos = dir->children;
            while (pos) {
                Ecma119Node *child;
                ret = create_tree(image, pos, &child);
                if (ret < 0) {
                    /* error */
                    ecma119_node_free(node);
                    return ret;
                } else if (ret == ISO_SUCCESS) {
                    /* add child to this node */
                    int nchildren = node->info.dir.nchildren++;
                    node->info.dir.children[nchildren] = child;
                    child->parent = node;
                }
                pos = pos->next;
            }
        }
        break;
    default:
        /* should never happen */
        return ISO_ERROR;
    }
    if (ret < 0) {
        return ret;
    }
    *tree = node;
    return ISO_SUCCESS;
}

int ecma119_tree_create(Ecma119Image *img, IsoNode *iso, Ecma119Node **tree)
{
    int ret;
    ret = create_tree(img, iso, tree);
    if (ret < 0) {
        return ret;
    }
    
    /*
     * TODO
     * - take care about dirs whose level is over 8
     * - sort files in dir
     * - mangle names
     */
    
    return ISO_SUCCESS;
}
