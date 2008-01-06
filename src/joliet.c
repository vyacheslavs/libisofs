/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2007 Mario Danic
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "joliet.h"
#include "messages.h"
#include "writer.h"
#include "error.h"
#include "image.h"
#include "filesrc.h"

#include <stdlib.h>

static
int get_joliet_name(Ecma119Image *t, IsoNode *iso, uint16_t **name)
{
    int ret;
    uint16_t *ucs_name;
    uint16_t *jname = NULL;

    if (iso->name == NULL) {
        /* it is not necessarily an error, it can be the root */
        return ISO_SUCCESS;
    }

    ret = str2ucs(t->input_charset, iso->name, &ucs_name);
    if (ret < 0) {
        iso_msg_debug(t->image->messenger, "Can't convert %s", iso->name);
        return ret;
    }

    // TODO add support for relaxed constraints
    jname = iso_j_id(ucs_name);
    free(ucs_name);
    if (jname != NULL) {
        *name = jname;
        return ISO_SUCCESS;
    } else {
        /* 
         * only possible if mem error, as check for empty names is done
         * in public tree
         */
        return ISO_MEM_ERROR;
    }
}

static
void joliet_node_free(JolietNode *node)
{
    if (node == NULL) {
        return;
    }
    if (node->type == JOLIET_DIR) {
        int i;
        for (i = 0; i < node->info.dir.nchildren; i++) {
            joliet_node_free(node->info.dir.children[i]);
        }
        free(node->info.dir.children);
        //free(node->info.dir);
    }
    iso_node_unref(node->node);
    free(node);
}

/**
 * Create a low level Joliet node
 * @return
 *      1 success, 0 ignored, < 0 error
 */
static
int create_node(Ecma119Image *t, IsoNode *iso, JolietNode **node)
{
    int ret;
    JolietNode *joliet;

    joliet = calloc(1, sizeof(JolietNode));
    if (joliet == NULL) {
        return ISO_MEM_ERROR;
    }

    if (iso->type == LIBISO_DIR) {
        IsoDir *dir = (IsoDir*) iso;
        joliet->info.dir.children = calloc(sizeof(void*), dir->nchildren);
        if (joliet->info.dir.children == NULL) {
            free(joliet);
            return ISO_MEM_ERROR;
        }
        joliet->type = JOLIET_DIR;
    } else if (iso->type == LIBISO_FILE) {
        /* it's a file */
        off_t size;
        IsoFileSrc *src;
        IsoFile *file = (IsoFile*) iso;

        size = iso_stream_get_size(file->stream);
        if (size > (off_t)0xffffffff) {
            iso_msg_note(t->image->messenger, LIBISO_FILE_IGNORED,
                         "File \"%s\" can't be added to image because is "
                         "greater than 4GB", iso->name);
            free(joliet);
            return 0;
        }

        ret = iso_file_src_create(t, file, &src);
        if (ret < 0) {
            free(joliet);
            return ret;
        }
        joliet->info.file = src;
        joliet->type = JOLIET_FILE;
    } else {
        /* should never happen */
        //TODO handle boot nodes?!?
        free(joliet);
        return ISO_ERROR;
    }

    /* take a ref to the IsoNode */
    joliet->node = iso;
    iso_node_ref(iso);

    return ISO_SUCCESS;
}

/**
 * Create the low level Joliet tree from the high level ISO tree.
 * 
 * @return
 *      1 success, 0 file ignored, < 0 error
 */
static
int create_tree(Ecma119Image *t, IsoNode *iso, JolietNode **tree, int pathlen)
{
    int ret, max_path;
    JolietNode *node;
    uint16_t *jname = NULL;

    if (t == NULL || iso == NULL || tree == NULL) {
        return ISO_NULL_POINTER;
    }

    if (iso->hidden & LIBISO_HIDE_ON_JOLIET) {
        /* file will be ignored */
        return 0;
    }
    ret = get_joliet_name(t, iso, &jname);
    if (ret < 0) {
        return ret;
    }
    max_path = pathlen + 1 + (jname ? ucslen(jname) * 2 : 0);
    if (!t->joliet_longer_paths && max_path > 240) {
        /*
         * Wow!! Joliet is even more restrictive than plain ISO-9660,
         * that allows up to 255 bytes!!
         */
        iso_msg_note(t->image->messenger, LIBISO_FILE_IGNORED,
                     "File \"%s\" can't be added to Joliet tree, because "
                     "its path length is larger than 240", iso->name);
        free(jname);
        return 0;
    }

    switch (iso->type) {
    case LIBISO_FILE:
        ret = create_node(t, iso, &node);
        break;
    case LIBISO_DIR:
        {
            IsoNode *pos;
            IsoDir *dir = (IsoDir*)iso;
            ret = create_node(t, iso, &node);
            if (ret < 0) {
                free(jname);
                return ret;
            }
            pos = dir->children;
            while (pos) {
                int cret;
                JolietNode *child;
                cret = create_tree(t, pos, &child, max_path);
                if (cret < 0) {
                    /* error */
                    joliet_node_free(node);
                    ret = cret;
                    break;
                } else if (cret == ISO_SUCCESS) {
                    /* add child to this node */
                    int nchildren = node->info.dir.nchildren++;
                    node->info.dir.children[nchildren] = child;
                    child->parent = node;
                }
                pos = pos->next;
            }
        }
        break;
    case LIBISO_BOOT:
        //TODO
        return 0;
        break;
    case LIBISO_SYMLINK:
    case LIBISO_SPECIAL:
        iso_msg_note(t->image->messenger, LIBISO_JOLIET_WRONG_FILE_TYPE, 
                     "Can't add %s to Joliet tree. This kind of files can only"
                     " be added to a Rock Ridget tree. Skipping.", iso->name);
        ret = 0;
        break;
    default:
        /* should never happen */
        return ISO_ERROR;
    }
    if (ret <= 0) {
        free(jname);
        return ret;
    }
    node->name = jname;
    *tree = node;
    return ISO_SUCCESS;
}

static int
cmp_node(const void *f1, const void *f2)
{
    JolietNode *f = *((JolietNode**)f1);
    JolietNode *g = *((JolietNode**)f2);
    return ucscmp(f->name, g->name);
}

static 
void sort_tree(JolietNode *root)
{
    size_t i;

    qsort(root->info.dir.children, root->info.dir.nchildren, 
          sizeof(void*), cmp_node);
    for (i = 0; i < root->info.dir.nchildren; i++) {
        JolietNode *child = root->info.dir.children[i];
        if (child->type == JOLIET_DIR)
            sort_tree(child);
    }
}

static
int joliet_tree_create(Ecma119Image *t)
{
    int ret;
    JolietNode *root;
    
    if (t == NULL) {
        return ISO_NULL_POINTER;
    }

    ret = create_tree(t, (IsoNode*)t->image->root, &root, 0);
    if (ret <= 0) {
        if (ret == 0) {
            /* unexpected error, root ignored!! This can't happen */
            ret = ISO_ERROR;
        }
        return ret;
    }
    
    /* the Joliet tree is stored in Ecma119Image target */
    t->joliet_root = root;

    iso_msg_debug(t->image->messenger, "Sorting the Joliet tree...");
    sort_tree(root);

    //iso_msg_debug(t->image->messenger, "Mangling Joliet names...");
    // FIXME ret = mangle_tree(t, 1);

    return ISO_SUCCESS;
}

static
int joliet_writer_compute_data_blocks(IsoImageWriter *writer)
{
    //TODO
    return -1;
}

static
int joliet_writer_write_vol_desc(IsoImageWriter *writer)
{
    //TODO
    return -1;
}

static
int joliet_writer_write_data(IsoImageWriter *writer)
{
    //TODO
    return -1;
}

static
int joliet_writer_free_data(IsoImageWriter *writer)
{
    /* free the Joliet tree */
    Ecma119Image *t = writer->target;
    joliet_node_free(t->joliet_root);
    return ISO_SUCCESS;
}

int joliet_writer_create(Ecma119Image *target)
{
    int ret;
    IsoImageWriter *writer;

    writer = malloc(sizeof(IsoImageWriter));
    if (writer == NULL) {
        return ISO_MEM_ERROR;
    }

    writer->compute_data_blocks = joliet_writer_compute_data_blocks;
    writer->write_vol_desc = joliet_writer_write_vol_desc;
    writer->write_data = joliet_writer_write_data;
    writer->free_data = joliet_writer_free_data;
    writer->data = NULL; //TODO store joliet tree here
    writer->target = target;

    iso_msg_debug(target->image->messenger, 
                  "Creating low level Joliet tree...");
    ret = joliet_tree_create(target);
    if (ret < 0) {
        return ret;
    }

    /* add this writer to image */
    target->writers[target->nwriters++] = writer;

    /* we need the volume descriptor */
    target->curblock++;
    return ISO_SUCCESS;
}
