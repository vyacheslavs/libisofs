/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2009 Thomas Schmitt
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 */

#include "ecma119_tree.h"
#include "ecma119.h"
#include "node.h"
#include "util.h"
#include "filesrc.h"
#include "messages.h"
#include "image.h"
#include "stream.h"
#include "eltorito.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static
int get_iso_name(Ecma119Image *img, IsoNode *iso, char **name)
{
    int ret, relaxed;
    char *ascii_name;
    char *isoname= NULL;

    if (iso->name == NULL) {
        /* it is not necessarily an error, it can be the root */
        return ISO_SUCCESS;
    }

    ret = str2ascii(img->input_charset, iso->name, &ascii_name);
    if (ret < 0) {
        iso_msg_submit(img->image->id, ret, 0, "Can't convert %s", iso->name);
        return ret;
    }

    if (img->allow_full_ascii) {
        relaxed = 2;
    } else {
        relaxed = (int)img->allow_lowercase;
    }
    if (iso->type == LIBISO_DIR) {
        if (img->max_37_char_filenames) {
            isoname = iso_r_dirid(ascii_name, 37, relaxed);
        } else if (img->iso_level == 1) {
            if (relaxed) {
                isoname = iso_r_dirid(ascii_name, 8, relaxed);
            } else {
                isoname = iso_1_dirid(ascii_name);
            }
        } else {
            if (relaxed) {
                isoname = iso_r_dirid(ascii_name, 8, relaxed);
            } else {
                isoname = iso_2_dirid(ascii_name);
            }
        }
    } else {
        if (img->max_37_char_filenames) {
            isoname = iso_r_fileid(ascii_name, 36, relaxed,
                                   (img->no_force_dots & 1) ? 0 : 1);
        } else if (img->iso_level == 1) {
            if (relaxed) {
                isoname = iso_r_fileid(ascii_name, 11, relaxed,
                                       (img->no_force_dots & 1) ? 0 : 1);
            } else {
                isoname = iso_1_fileid(ascii_name);
            }
        } else {
            if (relaxed) {
                isoname = iso_r_fileid(ascii_name, 30, relaxed,
                                       (img->no_force_dots & 1) ? 0 : 1);
            } else {
                isoname = iso_2_fileid(ascii_name);
            }
        }
    }
    free(ascii_name);
    if (isoname != NULL) {
        *name = isoname;
        return ISO_SUCCESS;
    } else {
        /*
         * only possible if mem error, as check for empty names is done
         * in public tree
         */
        return ISO_OUT_OF_MEM;
    }
}

static
int create_ecma119_node(Ecma119Image *img, IsoNode *iso, Ecma119Node **node)
{
    Ecma119Node *ecma;

    ecma = calloc(1, sizeof(Ecma119Node));
    if (ecma == NULL) {
        return ISO_OUT_OF_MEM;
    }

    ecma->node = iso;
    iso_node_ref(iso);
    ecma->nlink = 1;
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
    struct ecma119_dir_info *dir_info;

    children = calloc(1, sizeof(void*) * iso->nchildren);
    if (children == NULL) {
        return ISO_OUT_OF_MEM;
    }

    dir_info = calloc(1, sizeof(struct ecma119_dir_info));
    if (dir_info == NULL) {
        free(children);
        return ISO_OUT_OF_MEM;
    }

    ret = create_ecma119_node(img, (IsoNode*)iso, node);
    if (ret < 0) {
        free(children);
        free(dir_info);
        return ret;
    }
    (*node)->type = ECMA119_DIR;
    (*node)->info.dir = dir_info;
    (*node)->info.dir->nchildren = 0;
    (*node)->info.dir->children = children;
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
    off_t size;

    size = iso_stream_get_size(iso->stream);
    if (size > (off_t)MAX_ISO_FILE_SECTION_SIZE && img->iso_level != 3) {
        char *ipath = iso_tree_get_node_path(ISO_NODE(iso));
        ret = iso_msg_submit(img->image->id, ISO_FILE_TOO_BIG, 0,
                              "File \"%s\" can't be added to image because "
                              "is greater than 4GB", ipath);
        free(ipath);
        return ret;
    }

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

/**
 * Create a new ECMA-119 node representing a regular file from an El-Torito
 * boot catalog
 */
static
int create_boot_cat(Ecma119Image *img, IsoBoot *iso, Ecma119Node **node)
{
    int ret;
    IsoFileSrc *src;

    ret = el_torito_catalog_file_src_create(img, &src);
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

/**
 * Create a new ECMA-119 node representing a symbolic link from a iso symlink
 * node.
 */
static
int create_symlink(Ecma119Image *img, IsoSymlink *iso, Ecma119Node **node)
{
    int ret;

    ret = create_ecma119_node(img, (IsoNode*)iso, node);
    if (ret < 0) {
        return ret;
    }
    (*node)->type = ECMA119_SYMLINK;
    return ISO_SUCCESS;
}

/**
 * Create a new ECMA-119 node representing a special file.
 */
static
int create_special(Ecma119Image *img, IsoSpecial *iso, Ecma119Node **node)
{
    int ret;

    ret = create_ecma119_node(img, (IsoNode*)iso, node);
    if (ret < 0) {
        return ret;
    }
    (*node)->type = ECMA119_SPECIAL;
    return ISO_SUCCESS;
}

void ecma119_node_free(Ecma119Node *node)
{
    if (node == NULL) {
        return;
    }
    if (node->type == ECMA119_DIR) {
        int i;
        for (i = 0; i < node->info.dir->nchildren; i++) {
            ecma119_node_free(node->info.dir->children[i]);
        }
        free(node->info.dir->children);
        free(node->info.dir);
    }
    free(node->iso_name);
    iso_node_unref(node->node);
    free(node);
}

/**
 *
 * @return
 *      1 success, 0 node ignored,  < 0 error
 *
 */
static
int create_tree(Ecma119Image *image, IsoNode *iso, Ecma119Node **tree,
                int depth, int pathlen)
{
    int ret;
    Ecma119Node *node;
    int max_path;
    char *iso_name= NULL;

    if (image == NULL || iso == NULL || tree == NULL) {
        return ISO_NULL_POINTER;
    }

    if (iso->hidden & LIBISO_HIDE_ON_RR) {
        /* file will be ignored */
        return 0;
    }
    ret = get_iso_name(image, iso, &iso_name);
    if (ret < 0) {
        return ret;
    }
    max_path = pathlen + 1 + (iso_name ? strlen(iso_name) : 0);
    if (!image->rockridge) {
        if ((iso->type == LIBISO_DIR && depth > 8) && !image->allow_deep_paths) {
            char *ipath = iso_tree_get_node_path(iso);
            return iso_msg_submit(image->image->id, ISO_FILE_IMGPATH_WRONG, 0,
                         "File \"%s\" can't be added, because directory depth "
                         "is greater than 8.", ipath);
            free(iso_name);
            free(ipath);
            return ret;
        } else if (max_path > 255 && !image->allow_longer_paths) {
            char *ipath = iso_tree_get_node_path(iso);
            ret = iso_msg_submit(image->image->id, ISO_FILE_IMGPATH_WRONG, 0,
                         "File \"%s\" can't be added, because path length "
                         "is greater than 255 characters", ipath);
            free(iso_name);
            free(ipath);
            return ret;
        }
    }

    switch (iso->type) {
    case LIBISO_FILE:
        ret = create_file(image, (IsoFile*)iso, &node);
        break;
    case LIBISO_SYMLINK:
        if (image->rockridge) {
            ret = create_symlink(image, (IsoSymlink*)iso, &node);
        } else {
            /* symlinks are only supported when RR is enabled */
            char *ipath = iso_tree_get_node_path(iso);
            ret = iso_msg_submit(image->image->id, ISO_FILE_IGNORED, 0,
                "File \"%s\" ignored. Symlinks need RockRidge extensions.",
                ipath);
            free(ipath);
        }
        break;
    case LIBISO_SPECIAL:
        if (image->rockridge) {
            ret = create_special(image, (IsoSpecial*)iso, &node);
        } else {
            /* special files are only supported when RR is enabled */
            char *ipath = iso_tree_get_node_path(iso);
            ret = iso_msg_submit(image->image->id, ISO_FILE_IGNORED, 0,
                "File \"%s\" ignored. Special files need RockRidge extensions.",
                ipath);
            free(ipath);
        }
        break;
    case LIBISO_BOOT:
        if (image->eltorito) {
            ret = create_boot_cat(image, (IsoBoot*)iso, &node);
        } else {
            /* log and ignore */
            ret = iso_msg_submit(image->image->id, ISO_FILE_IGNORED, 0,
                "El-Torito catalog found on a image without El-Torito.");
        }
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
                int cret;
                Ecma119Node *child;
                cret = create_tree(image, pos, &child, depth + 1, max_path);
                if (cret < 0) {
                    /* error */
                    ecma119_node_free(node);
                    ret = cret;
                    break;
                } else if (cret == ISO_SUCCESS) {
                    /* add child to this node */
                    int nchildren = node->info.dir->nchildren++;
                    node->info.dir->children[nchildren] = child;
                    child->parent = node;
                }
                pos = pos->next;
            }
        }
        break;
    default:
        /* should never happen */
        return ISO_ASSERT_FAILURE;
    }
    if (ret <= 0) {
        free(iso_name);
        return ret;
    }
    node->iso_name = iso_name;
    *tree = node;
    return ISO_SUCCESS;
}

/**
 * Compare the iso name of two ECMA-119 nodes
 */
static
int cmp_node_name(const void *f1, const void *f2)
{
    Ecma119Node *f = *((Ecma119Node**)f1);
    Ecma119Node *g = *((Ecma119Node**)f2);
    return strcmp(f->iso_name, g->iso_name);
}

/**
 * Sorts a the children of each directory in the ECMA-119 tree represented
 * by \p root, acording to the order specified in ECMA-119, section 9.3.
 */
static
void sort_tree(Ecma119Node *root)
{
    size_t i;

    qsort(root->info.dir->children, root->info.dir->nchildren, sizeof(void*),
          cmp_node_name);
    for (i = 0; i < root->info.dir->nchildren; i++) {
        if (root->info.dir->children[i]->type == ECMA119_DIR)
            sort_tree(root->info.dir->children[i]);
    }
}

/**
 * Ensures that the ISO name of each children of the given dir is unique,
 * changing some of them if needed.
 * It also ensures that resulting filename is always <= than given
 * max_name_len, including extension. If needed, the extension will be reduced,
 * but never under 3 characters.
 */
static
int mangle_single_dir(Ecma119Image *img, Ecma119Node *dir, int max_file_len,
                      int max_dir_len)
{
    int ret;
    int i, nchildren;
    Ecma119Node **children;
    IsoHTable *table;
    int need_sort = 0;

    nchildren = dir->info.dir->nchildren;
    children = dir->info.dir->children;

    /* a hash table will temporary hold the names, for fast searching */
    ret = iso_htable_create((nchildren * 100) / 80, iso_str_hash,
                            (compare_function_t)strcmp, &table);
    if (ret < 0) {
        return ret;
    }
    for (i = 0; i < nchildren; ++i) {
        char *name = children[i]->iso_name;
        ret = iso_htable_add(table, name, name);
        if (ret < 0) {
            goto mangle_cleanup;
        }
    }

    for (i = 0; i < nchildren; ++i) {
        char *name, *ext;
        char full_name[40];
        int max; /* computed max len for name, without extension */
        int j = i;
        int digits = 1; /* characters to change per name */

        /* first, find all child with same name */
        while (j + 1 < nchildren && !cmp_node_name(children + i, children + j
                + 1)) {
            ++j;
        }
        if (j == i) {
            /* name is unique */
            continue;
        }

        /*
         * A max of 7 characters is good enought, it allows handling up to
         * 9,999,999 files with same name. We can increment this to
         * max_name_len, but the int_pow() function must then be modified
         * to return a bigger integer.
         */
        while (digits < 8) {
            int ok, k;
            char *dot;
            int change = 0; /* number to be written */

            /* copy name to buffer */
            strcpy(full_name, children[i]->iso_name);

            /* compute name and extension */
            dot = strrchr(full_name, '.');
            if (dot != NULL && children[i]->type != ECMA119_DIR) {

                /*
                 * File (not dir) with extension
                 * Note that we don't need to check for placeholders, as
                 * tree reparent happens later, so no placeholders can be
                 * here at this time.
                 */
                int extlen;
                full_name[dot - full_name] = '\0';
                name = full_name;
                ext = dot + 1;

                /*
                 * For iso level 1 we force ext len to be 3, as name
                 * can't grow on the extension space
                 */
                extlen = (max_file_len == 12) ? 3 : strlen(ext);
                max = max_file_len - extlen - 1 - digits;
                if (max <= 0) {
                    /* this can happen if extension is too long */
                    if (extlen + max > 3) {
                        /*
                         * reduce extension len, to give name an extra char
                         * note that max is negative or 0
                         */
                        extlen = extlen + max - 1;
                        ext[extlen] = '\0';
                        max = max_file_len - extlen - 1 - digits;
                    } else {
                        /*
                         * error, we don't support extensions < 3
                         * This can't happen with current limit of digits.
                         */
                        ret = ISO_ERROR;
                        goto mangle_cleanup;
                    }
                }
                /* ok, reduce name by digits */
                if (name + max < dot) {
                    name[max] = '\0';
                }
            } else {
                /* Directory, or file without extension */
                if (children[i]->type == ECMA119_DIR) {
                    max = max_dir_len - digits;
                    dot = NULL; /* dots have no meaning in dirs */
                } else {
                    max = max_file_len - digits;
                }
                name = full_name;
                if (max < strlen(name)) {
                    name[max] = '\0';
                }
                /* let ext be an empty string */
                ext = name + strlen(name);
            }

            ok = 1;
            /* change name of each file */
            for (k = i; k <= j; ++k) {
                char tmp[40];
                char fmt[16];
                if (dot != NULL) {
                    sprintf(fmt, "%%s%%0%dd.%%s", digits);
                } else {
                    sprintf(fmt, "%%s%%0%dd%%s", digits);
                }
                while (1) {
                    sprintf(tmp, fmt, name, change, ext);
                    ++change;
                    if (change > int_pow(10, digits)) {
                        ok = 0;
                        break;
                    }
                    if (!iso_htable_get(table, tmp, NULL)) {
                        /* the name is unique, so it can be used */
                        break;
                    }
                }
                if (ok) {
                    char *new = strdup(tmp);
                    if (new == NULL) {
                        ret = ISO_OUT_OF_MEM;
                        goto mangle_cleanup;
                    }
                    iso_msg_debug(img->image->id, "\"%s\" renamed to \"%s\"",
                                  children[k]->iso_name, new);

                    iso_htable_remove_ptr(table, children[k]->iso_name, NULL);
                    free(children[k]->iso_name);
                    children[k]->iso_name = new;
                    iso_htable_add(table, new, new);

                    /*
                     * if we change a name we need to sort again children
                     * at the end
                     */
                    need_sort = 1;
                } else {
                    /* we need to increment digits */
                    break;
                }
            }
            if (ok) {
                break;
            } else {
                ++digits;
            }
        }
        if (digits == 8) {
            ret = ISO_MANGLE_TOO_MUCH_FILES;
            goto mangle_cleanup;
        }
        i = j;
    }

    /*
     * If needed, sort again the files inside dir
     */
    if (need_sort) {
        qsort(children, nchildren, sizeof(void*), cmp_node_name);
    }

    ret = ISO_SUCCESS;

mangle_cleanup : ;
    iso_htable_destroy(table, NULL);
    return ret;
}

static
int mangle_dir(Ecma119Image *img, Ecma119Node *dir, int max_file_len,
               int max_dir_len)
{
    int ret;
    size_t i;

    ret = mangle_single_dir(img, dir, max_file_len, max_dir_len);
    if (ret < 0) {
        return ret;
    }

    /* recurse */
    for (i = 0; i < dir->info.dir->nchildren; ++i) {
        if (dir->info.dir->children[i]->type == ECMA119_DIR) {
            ret = mangle_dir(img, dir->info.dir->children[i], max_file_len,
                             max_dir_len);
            if (ret < 0) {
                /* error */
                return ret;
            }
        }
    }
    return ISO_SUCCESS;
}

static
int mangle_tree(Ecma119Image *img, int recurse)
{
    int max_file, max_dir;

    if (img->max_37_char_filenames) {
        max_file = max_dir = 37;
    } else if (img->iso_level == 1) {
        max_file = 12; /* 8 + 3 + 1 */
        max_dir = 8;
    } else {
        max_file = max_dir = 31;
    }
    if (recurse) {
        return mangle_dir(img, img->root, max_file, max_dir);
    } else {
        return mangle_single_dir(img, img->root, max_file, max_dir);
    }
}

/**
 * Create a new ECMA-119 node representing a placeholder for a relocated
 * dir.
 *
 * See IEEE P1282, section 4.1.5 for details
 */
static
int create_placeholder(Ecma119Node *parent, Ecma119Node *real,
                       Ecma119Node **node)
{
    Ecma119Node *ret;

    ret = calloc(1, sizeof(Ecma119Node));
    if (ret == NULL) {
        return ISO_OUT_OF_MEM;
    }

    /*
     * TODO
     * If real is a dir, while placeholder is a file, ISO name restricctions
     * are different, what to do?
     */
    ret->iso_name = strdup(real->iso_name);
    if (ret->iso_name == NULL) {
        free(ret);
        return ISO_OUT_OF_MEM;
    }

    /* take a ref to the IsoNode */
    ret->node = real->node;
    iso_node_ref(real->node);
    ret->parent = parent;
    ret->type = ECMA119_PLACEHOLDER;
    ret->info.real_me = real;
    ret->ino = real->ino;
    ret->nlink = real->nlink;

    *node = ret;
    return ISO_SUCCESS;
}

static
size_t max_child_name_len(Ecma119Node *dir)
{
    size_t ret = 0, i;
    for (i = 0; i < dir->info.dir->nchildren; i++) {
        size_t len = strlen(dir->info.dir->children[i]->iso_name);
        ret = MAX(ret, len);
    }
    return ret;
}

/**
 * Relocates a directory, as specified in Rock Ridge Specification
 * (see IEEE P1282, section 4.1.5). This is needed when the number of levels
 * on a directory hierarchy exceeds 8, or the length of a path is higher
 * than 255 characters, as specified in ECMA-119, section 6.8.2.1
 */
static
int reparent(Ecma119Node *child, Ecma119Node *parent)
{
    int ret;
    size_t i;
    Ecma119Node *placeholder;

    /* replace the child in the original parent with a placeholder */
    for (i = 0; i < child->parent->info.dir->nchildren; i++) {
        if (child->parent->info.dir->children[i] == child) {
            ret = create_placeholder(child->parent, child, &placeholder);
            if (ret < 0) {
                return ret;
            }
            child->parent->info.dir->children[i] = placeholder;
            break;
        }
    }

    /* just for debug, this should never happen... */
    if (i == child->parent->info.dir->nchildren) {
        return ISO_ASSERT_FAILURE;
    }

    /* keep track of the real parent */
    child->info.dir->real_parent = child->parent;

    /* add the child to its new parent */
    child->parent = parent;
    parent->info.dir->nchildren++;
    parent->info.dir->children = realloc(parent->info.dir->children,
                                 sizeof(void*) * parent->info.dir->nchildren);
    parent->info.dir->children[parent->info.dir->nchildren - 1] = child;
    return ISO_SUCCESS;
}

/**
 * Reorder the tree, if necessary, to ensure that
 *  - the depth is at most 8
 *  - each path length is at most 255 characters
 * This restriction is imposed by ECMA-119 specification (ECMA-119, 6.8.2.1).
 *
 * @param dir
 *      Dir we are currently processing
 * @param level
 *      Level of the directory in the hierarchy
 * @param pathlen
 *      Length of the path until dir, including it
 * @return
 *      1 success, < 0 error
 */
static
int reorder_tree(Ecma119Image *img, Ecma119Node *dir, int level, int pathlen)
{
    int ret;
    size_t max_path;

    max_path = pathlen + 1 + max_child_name_len(dir);

    if (level > 8 || max_path > 255) {
        ret = reparent(dir, img->root);
        if (ret < 0) {
            return ret;
        }

        /*
         * we are appended to the root's children now, so there is no
         * need to recurse (the root will hit us again)
         */
    } else {
        size_t i;

        for (i = 0; i < dir->info.dir->nchildren; i++) {
            Ecma119Node *child = dir->info.dir->children[i];
            if (child->type == ECMA119_DIR) {
                int newpathlen = pathlen + 1 + strlen(child->iso_name);
                ret = reorder_tree(img, child, level + 1, newpathlen);
                if (ret < 0) {
                    return ret;
                }
            }
        }
    }
    return ISO_SUCCESS;
}

/*
 * @param flag
 *     bit0= recursion
 *     bit1= count nodes rather than fill them into *nodes
 * @return
 *     <0 error
 *     bit0= saw ino == 0
 *     bit1= saw ino != 0
 */
static
int make_node_array(Ecma119Image *img, Ecma119Node *dir,
                    Ecma119Node **nodes, size_t nodes_size, size_t *node_count,
                    int flag)
{
    int ret, result = 0;
    size_t i;
    Ecma119Node *child;

    if (!(flag & 1)) {
        *node_count = 0;
        if (!(flag & 2)) {
            /* Register the tree root node */
            if (*node_count >= nodes_size) {
                iso_msg_submit(img->image->id, ISO_ASSERT_FAILURE, 0,
                         "Programming error: Overflow of hardlink sort array");
                return ISO_ASSERT_FAILURE;
            }
            nodes[*node_count] = dir;
        }
        result|= (dir->ino == 0 ? 1 : 2);
        (*node_count)++;
    }
        
    for (i = 0; i < dir->info.dir->nchildren; i++) {
        child = dir->info.dir->children[i];
        if (!(flag & 2)) {
            if (*node_count >= nodes_size) {
                iso_msg_submit(img->image->id, ISO_ASSERT_FAILURE, 0,
                         "Programming error: Overflow of hardlink sort array");
                return ISO_ASSERT_FAILURE;
            }
            nodes[*node_count] = child;
        }
        result|= (child->ino == 0 ? 1 : 2);
        (*node_count)++;

        if (child->type == ECMA119_DIR) {
            ret = make_node_array(img, child,
                                  nodes, nodes_size, node_count, flag | 1);
            if (ret < 0)
                return ret;
        }
    }
    return result;
}

/*
 * @param flag
 *     bit0= compare stat properties and attributes 
 *     bit1= treat all nodes with image ino == 0 as unique
 */
static
int ecma119_node_cmp_flag(const void *v1, const void *v2, int flag)
{
    int ret;
    Ecma119Node *n1, *n2;

    n1 = *((Ecma119Node **) v1);
    n2 = *((Ecma119Node **) v2);
    if (n1 == n2)
        return 0;

    ret = iso_node_cmp_flag(n1->node, n2->node, flag & (1 | 2));
    return ret;
}

static 
int ecma119_node_cmp_hard(const void *v1, const void *v2)
{
    return ecma119_node_cmp_flag(v1, v2, 1);
}   

static 
int ecma119_node_cmp_nohard(const void *v1, const void *v2)
{
    return ecma119_node_cmp_flag(v1, v2, 1 | 2);
}   

static
int family_set_ino(Ecma119Image *img, Ecma119Node **nodes, size_t family_start,
                   size_t next_family, ino_t img_ino, ino_t prev_ino, int flag)
{
    size_t i;

    if (img_ino != 0) {
        /* Check whether this is the same img_ino as in the previous
           family (e.g. by property divergence of imported hardlink).
        */
        if (img_ino == prev_ino)
            img_ino = 0;
    }
    if (img_ino == 0) {
        img_ino = img_give_ino_number(img->image, 0);
    }
    for (i = family_start; i < next_family; i++) {
        nodes[i]->ino = img_ino;
        nodes[i]->nlink = next_family - family_start;
    }
    return 1;
}

static
int match_hardlinks(Ecma119Image *img, Ecma119Node *dir, int flag)
{
    int ret;
    size_t nodes_size = 0, node_count = 0, i, family_start;
    Ecma119Node **nodes = NULL;
    unsigned int fs_id;
    dev_t dev_id;
    ino_t img_ino = 0, prev_ino = 0;

    ret = make_node_array(img, dir, nodes, nodes_size, &node_count, 2);
    if (ret < 0)
        return ret;
    nodes_size = node_count;
    nodes = (Ecma119Node **) calloc(sizeof(Ecma119Node *), nodes_size);
    if (nodes == NULL)
        return ISO_OUT_OF_MEM;
    ret = make_node_array(img, dir, nodes, nodes_size, &node_count, 0);
    if (ret < 0)
        goto ex;

    /* Sort according to id tuples, IsoFileSrc identity, properties, xattr. */
    if (img->hardlinks)
        qsort(nodes, node_count, sizeof(Ecma119Node *), ecma119_node_cmp_hard);
    else
        qsort(nodes, node_count, sizeof(Ecma119Node *),
              ecma119_node_cmp_nohard);

    /* Hand out image inode numbers to all Ecma119Node.ino == 0 .
       Same sorting rank gets same inode number.
       Split those image inode number families where the sort criterion
       differs.
    */
    iso_node_get_id(nodes[0]->node, &fs_id, &dev_id, &img_ino, 1);
    family_start = 0;
    for (i = 1; i < node_count; i++) {
        if (ecma119_node_cmp_hard(nodes + (i - 1), nodes + i) == 0) {
            /* Still in same ino family */
            if (img_ino == 0) { /* Just in case any member knows its img_ino */
                iso_node_get_id(nodes[0]->node, &fs_id, &dev_id, &img_ino, 1);
           }
    continue;
        }
        family_set_ino(img, nodes, family_start, i, img_ino, prev_ino, 0);
        prev_ino = img_ino;
        iso_node_get_id(nodes[i]->node, &fs_id, &dev_id, &img_ino, 1);
        family_start = i;
    }
    family_set_ino(img, nodes, family_start, i, img_ino, prev_ino, 0);

    ret = ISO_SUCCESS;
ex:;
    if (nodes != NULL)
        free((char *) nodes);
    return ret;
}

int ecma119_tree_create(Ecma119Image *img)
{
    int ret;
    Ecma119Node *root;

    ret = create_tree(img, (IsoNode*)img->image->root, &root, 1, 0);
    if (ret <= 0) {
        if (ret == 0) {
            /* unexpected error, root ignored!! This can't happen */
            ret = ISO_ASSERT_FAILURE;
        }
        return ret;
    }
    img->root = root;

    iso_msg_debug(img->image->id, "Matching hardlinks...");
    ret = match_hardlinks(img, img->root, 0);
    if (ret < 0) {
        return ret;
    }

    iso_msg_debug(img->image->id, "Sorting the low level tree...");
    sort_tree(root);

    iso_msg_debug(img->image->id, "Mangling names...");
    ret = mangle_tree(img, 1);
    if (ret < 0) {
        return ret;
    }

    if (img->rockridge && !img->allow_deep_paths) {

        /* reorder the tree, acording to RRIP, 4.1.5 */
        ret = reorder_tree(img, img->root, 1, 0);
        if (ret < 0) {
            return ret;
        }

        /*
         * and we need to remangle the root directory, as the function
         * above could insert new directories into the root.
         * Note that recurse = 0, as we don't need to recurse.
         */
        ret = mangle_tree(img, 0);
        if (ret < 0) {
            return ret;
        }
    }

    return ISO_SUCCESS;
}
