/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2007 Mario Danic
 * Copyright (c) 2011-2012 Thomas Schmitt
 * Copyright (c) 2012 Vladimir Serbinenko
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "hfsplus.h"
#include "messages.h"
#include "writer.h"
#include "image.h"
#include "filesrc.h"
#include "eltorito.h"
#include "libisofs.h"
#include "util.h"
#include "ecma119.h"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HFSPLUS_BLOCK_SIZE BLOCK_SIZE

static
int get_hfsplus_name(Ecma119Image *t, IsoNode *iso, uint16_t **name)
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
        iso_msg_debug(t->image->id, "Can't convert %s", iso->name);
        return ret;
    }
    /* FIXME: Decompose it.  */
    jname = ucs_name;
    if (jname != NULL) {
        *name = jname;
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
void hfsplus_node_free(HFSPlusNode *node)
{
    if (node == NULL) {
        return;
    }
    if (node->type == HFSPLUS_DIR) {
        size_t i;
        for (i = 0; i < node->info.dir->nchildren; i++) {
            hfsplus_node_free(node->info.dir->children[i]);
        }
        free(node->info.dir->children);
        free(node->info.dir);
    }
    iso_node_unref(node->node);
    free(node->name);
    free(node);
}

/**
 * Create a low level Hfsplus node
 * @return
 *      1 success, 0 ignored, < 0 error
 */
static
int create_node(Ecma119Image *t, IsoNode *iso, HFSPlusNode **node)
{
    int ret;
    HFSPlusNode *hfsplus;

    hfsplus = calloc(1, sizeof(HFSPlusNode));
    if (hfsplus == NULL) {
        return ISO_OUT_OF_MEM;
    }

    if (iso->type == LIBISO_DIR) {
        IsoDir *dir = (IsoDir*) iso;
        hfsplus->info.dir = calloc(1, sizeof(struct hfsplus_dir_info));
        if (hfsplus->info.dir == NULL) {
            free(hfsplus);
            return ISO_OUT_OF_MEM;
        }
        hfsplus->info.dir->children = calloc(sizeof(void*), dir->nchildren);
        if (hfsplus->info.dir->children == NULL) {
            free(hfsplus->info.dir);
            free(hfsplus);
            return ISO_OUT_OF_MEM;
        }
        hfsplus->type = HFSPLUS_DIR;
    } else if (iso->type == LIBISO_FILE) {
        /* it's a file */
        off_t size;
        IsoFileSrc *src;
        IsoFile *file = (IsoFile*) iso;

        size = iso_stream_get_size(file->stream);
        if (size > (off_t)MAX_ISO_FILE_SECTION_SIZE && t->iso_level != 3) {
            char *ipath = iso_tree_get_node_path(iso);
            free(hfsplus);
            ret = iso_msg_submit(t->image->id, ISO_FILE_TOO_BIG, 0,
                         "File \"%s\" can't be added to image because is "
                         "greater than 4GB", ipath);
            free(ipath);
            return ret;
        }

        ret = iso_file_src_create(t, file, &src);
        if (ret < 0) {
            free(hfsplus);
            return ret;
        }
        hfsplus->info.file = src;
        hfsplus->type = HFSPLUS_FILE;
    } else if (iso->type == LIBISO_BOOT) {
        /* it's a el-torito boot catalog, that we write as a file */
        IsoFileSrc *src;

        ret = el_torito_catalog_file_src_create(t, &src);
        if (ret < 0) {
            free(hfsplus);
            return ret;
        }
        hfsplus->info.file = src;
        hfsplus->type = HFSPLUS_FILE;
    } else {
        /* should never happen */
        free(hfsplus);
        return ISO_ASSERT_FAILURE;
    }

    /* take a ref to the IsoNode */
    hfsplus->node = iso;
    iso_node_ref(iso);

    *node = hfsplus;
    return ISO_SUCCESS;
}

/**
 * Create the low level Hfsplus tree from the high level ISO tree.
 *
 * @return
 *      1 success, 0 file ignored, < 0 error
 */
static
int create_tree(Ecma119Image *t, IsoNode *iso, HFSPlusNode **tree, int pathlen)
{
    int ret, max_path;
    HFSPlusNode *node = NULL;
    uint16_t *jname = NULL;

    if (t == NULL || iso == NULL || tree == NULL) {
        return ISO_NULL_POINTER;
    }

    if (iso->hidden & LIBISO_HIDE_ON_HFSPLUS) {
        /* file will be ignored */
        return 0;
    }
    ret = get_hfsplus_name(t, iso, &jname);
    if (ret < 0) {
        return ret;
    }
    max_path = pathlen + 1 + (jname ? ucslen(jname) * 2 : 0);

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
                HFSPlusNode *child;
                cret = create_tree(t, pos, &child, max_path);
                if (cret < 0) {
                    /* error */
                    hfsplus_node_free(node);
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
    case LIBISO_BOOT:
        if (t->eltorito) {
            ret = create_node(t, iso, &node);
        } else {
            /* log and ignore */
            ret = iso_msg_submit(t->image->id, ISO_FILE_IGNORED, 0,
                "El-Torito catalog found on a image without El-Torito.");
        }
        break;
    case LIBISO_SYMLINK:
    case LIBISO_SPECIAL:
        {
            char *ipath = iso_tree_get_node_path(iso);
            ret = iso_msg_submit(t->image->id, ISO_FILE_IGNORED, 0,
                 "Can't add %s to Hfsplus tree. %s can only be added to a "
                 "Rock Ridge tree.", ipath, (iso->type == LIBISO_SYMLINK ?
                                             "Symlinks" : "Special files"));
            free(ipath);
        }
        break;
    default:
        /* should never happen */
        return ISO_ASSERT_FAILURE;
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
    HFSPlusNode *f = *((HFSPlusNode**)f1);
    HFSPlusNode *g = *((HFSPlusNode**)f2);
    return ucscmp(f->name, g->name);
}

static
void sort_tree(HFSPlusNode *root)
{
    size_t i;

    qsort(root->info.dir->children, root->info.dir->nchildren,
          sizeof(void*), cmp_node);
    for (i = 0; i < root->info.dir->nchildren; i++) {
        HFSPlusNode *child = root->info.dir->children[i];
        if (child->type == HFSPLUS_DIR)
            sort_tree(child);
    }
}

static
int cmp_node_name(const void *f1, const void *f2)
{
    HFSPlusNode *f = *((HFSPlusNode**)f1);
    HFSPlusNode *g = *((HFSPlusNode**)f2);
    return ucscmp(f->name, g->name);
}

static
int hfsplus_create_mangled_name(uint16_t *dest, uint16_t *src, int digits,
                                int number, uint16_t *ext)
{
    int ret, pos;
    uint16_t *ucsnumber;
    char fmt[16];
    char nstr[72];
              /* was: The only caller of this function allocates dest
                      with 66 elements and limits digits to < 8
                 But this does not match the usage of nstr which has to take
                 the decimal representation of an int.
              */

    if (digits >= 8)
        return ISO_ASSERT_FAILURE;

    sprintf(fmt, "%%0%dd", digits);
    sprintf(nstr, fmt, number);

    ret = str2ucs("ASCII", nstr, &ucsnumber);
    if (ret < 0) {
        return ret;
    }

    /* copy name */
    pos = ucslen(src);
    ucsncpy(dest, src, pos);

    /* copy number */
    ucsncpy(dest + pos, ucsnumber, digits);
    pos += digits;

    if (ext[0] != (uint16_t)0) {
        size_t extlen = ucslen(ext);
        dest[pos++] = (uint16_t)0x2E00; /* '.' in big endian UCS */
        ucsncpy(dest + pos, ext, extlen);
        pos += extlen;
    }
    dest[pos] = (uint16_t)0;
    free(ucsnumber);
    return ISO_SUCCESS;
}

static
int mangle_single_dir(Ecma119Image *t, HFSPlusNode *dir)
{
    int ret;
    int i, nchildren, maxchar = 255;
    HFSPlusNode **children;
    IsoHTable *table;
    int need_sort = 0;
    uint16_t *full_name = NULL;
    uint16_t *tmp = NULL;

    LIBISO_ALLOC_MEM(full_name, uint16_t, LIBISO_HFSPLUS_NAME_MAX);
    LIBISO_ALLOC_MEM(tmp, uint16_t, LIBISO_HFSPLUS_NAME_MAX);
    nchildren = dir->info.dir->nchildren;
    children = dir->info.dir->children;

    /* a hash table will temporary hold the names, for fast searching */
    ret = iso_htable_create((nchildren * 100) / 80, iso_str_hash,
                            (compare_function_t)ucscmp, &table);
    if (ret < 0) {
        goto ex;
    }
    for (i = 0; i < nchildren; ++i) {
        uint16_t *name = children[i]->name;
        ret = iso_htable_add(table, name, name);
        if (ret < 0) {
            goto mangle_cleanup;
        }
    }

    for (i = 0; i < nchildren; ++i) {
        uint16_t *name, *ext;
        int max; /* computed max len for name, without extension */
        int j = i;
        int digits = 1; /* characters to change per name */

        /* first, find all child with same name */
        while (j + 1 < nchildren &&
                !cmp_node_name(children + i, children + j + 1)) {
            ++j;
        }
        if (j == i) {
            /* name is unique */
            continue;
        }

        /*
         * A max of 7 characters is good enought, it allows handling up to
         * 9,999,999 files with same name.
         */
         /* Important: hfsplus_create_mangled_name() relies on digits < 8 */

        while (digits < 8) {
            int ok, k;
            uint16_t *dot;
            int change = 0; /* number to be written */

            /* copy name to buffer */
            ucscpy(full_name, children[i]->name);

            /* compute name and extension */
            dot = ucsrchr(full_name, '.');
            if (dot != NULL && children[i]->type != HFSPLUS_DIR) {

                /*
                 * File (not dir) with extension
                 */
                int extlen;
                full_name[dot - full_name] = 0;
                name = full_name;
                ext = dot + 1;

                extlen = ucslen(ext);
                max = maxchar + 1 - extlen - 1 - digits;
                if (max <= 0) {
                    /* this can happen if extension is too long */
                    if (extlen + max > 3) {
                        /*
                         * reduce extension len, to give name an extra char
                         * note that max is negative or 0
                         */
                        extlen = extlen + max - 1;
                        ext[extlen] = 0;
                        max = maxchar + 2 - extlen - 1 - digits;
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
                    name[max] = 0;
                }
            } else {
                /* Directory, or file without extension */
                if (children[i]->type == HFSPLUS_DIR) {
                    max = maxchar + 1 - digits;
                    dot = NULL; /* dots have no meaning in dirs */
                } else {
                    max = maxchar + 1 - digits;
                }
                name = full_name;
                if ((size_t) max < ucslen(name)) {
                    name[max] = 0;
                }
                /* let ext be an empty string */
                ext = name + ucslen(name);
            }

            ok = 1;
            /* change name of each file */
            for (k = i; k <= j; ++k) {
                while (1) {
                    ret = hfsplus_create_mangled_name(tmp, name, digits,
                                                     change, ext);
                    if (ret < 0) {
                        goto mangle_cleanup;
                    }
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
                    uint16_t *new = ucsdup(tmp);
                    if (new == NULL) {
                        ret = ISO_OUT_OF_MEM;
                        goto mangle_cleanup;
                    }

                    iso_htable_remove_ptr(table, children[k]->name, NULL);
                    free(children[k]->name);
                    children[k]->name = new;
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
ex:;
    iso_htable_destroy(table, NULL);
    LIBISO_FREE_MEM(tmp);
    LIBISO_FREE_MEM(full_name);
    return ret;
}

static
int mangle_tree(Ecma119Image *t, HFSPlusNode *dir)
{
    int ret;
    size_t i;

    ret = mangle_single_dir(t, dir);
    if (ret < 0) {
        return ret;
    }

    /* recurse */
    for (i = 0; i < dir->info.dir->nchildren; ++i) {
        if (dir->info.dir->children[i]->type == HFSPLUS_DIR) {
            ret = mangle_tree(t, dir->info.dir->children[i]);
            if (ret < 0) {
                /* error */
                return ret;
            }
        }
    }
    return ISO_SUCCESS;
}

static
int hfsplus_tree_create(Ecma119Image *t)
{
    int ret;
    HFSPlusNode *root;

    if (t == NULL) {
        return ISO_NULL_POINTER;
    }

    ret = create_tree(t, (IsoNode*)t->image->root, &root, 0);
    if (ret <= 0) {
        if (ret == 0) {
            /* unexpected error, root ignored!! This can't happen */
            ret = ISO_ASSERT_FAILURE;
        }
        return ret;
    }

    /* the Hfsplus tree is stored in Ecma119Image target */
    t->hfsplus_root = root;

    iso_msg_debug(t->image->id, "Sorting the Hfsplus tree...");
    sort_tree(root);

    iso_msg_debug(t->image->id, "Mangling Hfsplus names...");
    ret = mangle_tree(t, root);
    if (ret < 0)
        return ret;
    return ISO_SUCCESS;
}

/**
 * Compute the size of a directory entry for a single node
 */
static
size_t calc_dirent_len(Ecma119Image *t, HFSPlusNode *n)
{
    /* note than name len is always even, so we always need the pad byte */
    int ret = n->name ? ucslen(n->name) * 2 + 34 : 34;
    if (n->type == HFSPLUS_FILE && !(t->omit_version_numbers & 3)) {
        /* take into account version numbers */
        ret += 4;
    }
    return ret;
}

/**
 * Computes the total size of all directory entries of a single hfsplus dir.
 * This is like ECMA-119 6.8.1.1, but taking care that names are stored in
 * UCS.
 */
static
size_t calc_dir_size(Ecma119Image *t, HFSPlusNode *dir)
{
    size_t i, len;

    /* size of "." and ".." entries */
    len = 34 + 34;

    for (i = 0; i < dir->info.dir->nchildren; ++i) {
        size_t remaining;
        int section, nsections;
        HFSPlusNode *child = dir->info.dir->children[i];
        size_t dirent_len = calc_dirent_len(t, child);

        nsections = (child->type == HFSPLUS_FILE) ? child->info.file->nsections : 1;
        for (section = 0; section < nsections; ++section) {
            remaining = HFSPLUS_BLOCK_SIZE - (len % HFSPLUS_BLOCK_SIZE);
            if (dirent_len > remaining) {
                /* child directory entry doesn't fit on block */
                len += remaining + dirent_len;
            } else {
                len += dirent_len;
            }
        }
    }

    /*
     * The size of a dir is always a multiple of block size, as we must add
     * the size of the unused space after the last directory record
     * (ECMA-119, 6.8.1.3)
     */
    len = ROUND_UP(len, HFSPLUS_BLOCK_SIZE);

    /* cache the len */
    dir->info.dir->len = len;
    return len;
}

static
void calc_dir_pos(Ecma119Image *t, HFSPlusNode *dir)
{
    size_t i, len;

    t->hfsp_ndirs++;
    dir->info.dir->block = t->curblock;
    dir->cat_id = t->hfsp_cat_id++;
    len = calc_dir_size(t, dir);
    t->curblock += DIV_UP(len, HFSPLUS_BLOCK_SIZE);
    for (i = 0; i < dir->info.dir->nchildren; i++) {
        HFSPlusNode *child = dir->info.dir->children[i];
        if (child->type == HFSPLUS_DIR)
	  calc_dir_pos(t, child);
        else
	  {
	    child->cat_id = t->hfsp_cat_id++;
	    t->hfsp_nfiles++;
	  }
    }
}

static
int hfsplus_writer_compute_data_blocks(IsoImageWriter *writer)
{
    Ecma119Image *t;
    uint32_t old_curblock, total_size;

    if (writer == NULL) {
        return ISO_OUT_OF_MEM;
    }

    t = writer->target;
    old_curblock = t->curblock;

    /* compute position of directories */
    iso_msg_debug(t->image->id, "Computing position of Hfsplus dir structure");
    calc_dir_pos(t, t->hfsplus_root);

    /* We need one bit for every block. */
    /* So if we allocate x blocks we have to satisfy:
       8 * HFSPLUS_BLOCK_SIZE * x >= total_size + x
       (8 * HFSPLUS_BLOCK_SIZE - 1) * x >= total_size
     */
    total_size = t->total_size + t->curblock - old_curblock;
    t->hfsp_allocation_blocks = total_size / (8 * HFSPLUS_BLOCK_SIZE - 1) + 1;
    t->curblock += t->hfsp_allocation_blocks;

    return ISO_SUCCESS;
}

/**
 * Write a single directory record for Hfsplus. It is like (ECMA-119, 9.1),
 * but file identifier is stored in UCS.
 *
 * @param file_id
 *     if >= 0, we use it instead of the filename (for "." and ".." entries).
 * @param len_fi
 *     Computed length of the file identifier. Total size of the directory
 *     entry will be len + 34 (ECMA-119, 9.1.12), as padding is always needed
 */
static
void write_one_dir_record(Ecma119Image *t, HFSPlusNode *node, int file_id,
                          uint8_t *buf, size_t len_fi, int extent)
{
    uint32_t len;
    uint32_t block;
    uint8_t len_dr; /*< size of dir entry */
    int multi_extend = 0;
    uint8_t *name = (file_id >= 0) ? (uint8_t*)&file_id
            : (uint8_t*)node->name;

    struct ecma119_dir_record *rec = (struct ecma119_dir_record*)buf;
    IsoNode *iso;

    len_dr = 33 + len_fi + ((len_fi % 2) ? 0 : 1);

    memcpy(rec->file_id, name, len_fi);

    if (node->type == HFSPLUS_FILE && !(t->omit_version_numbers & 3)) {
        len_dr += 4;
        rec->file_id[len_fi++] = 0;
        rec->file_id[len_fi++] = ';';
        rec->file_id[len_fi++] = 0;
        rec->file_id[len_fi++] = '1';
    }

    if (node->type == HFSPLUS_DIR) {
        /* use the cached length */
        len = node->info.dir->len;
        block = node->info.dir->block;
    } else if (node->type == HFSPLUS_FILE) {
        block = node->info.file->sections[extent].block;
        len = node->info.file->sections[extent].size;
        multi_extend = (node->info.file->nsections - 1 == extent) ? 0 : 1;
    } else {
        /*
         * for nodes other than files and dirs, we set both
         * len and block to 0
         */
        len = 0;
        block = 0;
    }

    /*
     * For ".." entry we need to write the parent info!
     */
    if (file_id == 1 && node->parent)
        node = node->parent;

    rec->len_dr[0] = len_dr;
    iso_bb(rec->block, block - t->eff_partition_offset, 4);
    iso_bb(rec->length, len, 4);

    /* was: iso_datetime_7(rec->recording_time, t->now, t->always_gmt);
    */
    iso= node->node;
    iso_datetime_7(rec->recording_time, 
                   (t->dir_rec_mtime & 2) ? ( t->replace_timestamps ?
                                              t->timestamp : iso->mtime )
                                          : t->now, t->always_gmt);

    rec->flags[0] = ((node->type == HFSPLUS_DIR) ? 2 : 0) | (multi_extend ? 0x80 : 0);
    iso_bb(rec->vol_seq_number, (uint32_t) 1, 2);
    rec->len_fi[0] = len_fi;
}

/**
 * Copy up to \p max characters from \p src to \p dest. If \p src has less than
 * \p max characters, we pad dest with " " characters.
 */
static
void ucsncpy_pad(uint16_t *dest, const uint16_t *src, size_t max)
{
    char *cdest, *csrc;
    size_t len, i;

    cdest = (char*)dest;
    csrc = (char*)src;

    if (src != NULL) {
        len = MIN(ucslen(src) * 2, max);
    } else {
        len = 0;
    }

    for (i = 0; i < len; ++i)
        cdest[i] = csrc[i];

    for (i = len; i < max; i += 2) {
        cdest[i] = '\0';
        cdest[i + 1] = ' ';
    }
}

static void set_time (uint32_t *tm, Ecma119Image *t)
{
  iso_msb ((uint8_t *) tm, t->now + 2082844800, 4);
}

int hfsplus_writer_write_vol_desc(IsoImageWriter *writer)
{
  char buffer[1024];
  int ret;

  IsoImage *image;
  Ecma119Image *t;
  struct hfsplus_volheader sb;

  if (writer == NULL) {
    return ISO_OUT_OF_MEM;
  }

  t = writer->target;
  image = t->image;

  t->hfsp_part_start = t->bytes_written / 0x800;

  memset (buffer, 0, sizeof (buffer));
  ret = iso_write(t, buffer, 1024);
  if (ret < 0)
    return ret;

  iso_msg_debug(image->id, "Write HFS+ superblock");

  memset (&sb, 0, sizeof (sb));

  iso_msb ((uint8_t *) &sb.magic, 0x482b, 2);
  iso_msb ((uint8_t *) &sb.version, 4, 2);
  /* Cleanly unmounted, software locked.  */
  iso_msb ((uint8_t *) &sb.attributes, (1 << 8) | (1 << 15), 4);
  iso_msb ((uint8_t *) &sb.last_mounted_version, 0x6c69736f, 4);
  set_time (&sb.ctime, t);
  set_time (&sb.utime, t);
  set_time (&sb.backup_time, t);
  set_time (&sb.fsck_time, t);
  iso_msb ((uint8_t *) &sb.file_count, t->hfsp_nfiles, 4);
  iso_msb ((uint8_t *) &sb.folder_count, t->hfsp_ndirs, 4);
  iso_msb ((uint8_t *) &sb.blksize, 0x800, 4);
  iso_msb ((uint8_t *) &sb.catalog_node_id, t->hfsp_cat_id, 4);
  iso_msb ((uint8_t *) &sb.rsrc_clumpsize, 0x800, 4);
  iso_msb ((uint8_t *) &sb.data_clumpsize, 0x800, 4);
  iso_msb ((uint8_t *) &sb.total_blocks, t->total_size / 0x800 - t->hfsp_part_start, 4);
  /*  
  uint64_t encodings_bitmap;
  uint32_t ppc_bootdir;
  uint32_t intel_bootfile;
  uint32_t showfolder;
  uint32_t os9folder;
  uint32_t unused;
  uint32_t osxfolder;
  uint64_t num_serial;
  struct hfsplus_forkdata allocations_file;
  struct hfsplus_forkdata extents_file;
  struct hfsplus_forkdata catalog_file;
  struct hfsplus_forkdata attrib_file;
  struct hfsplus_forkdata startup_file;*/

  ret = iso_write(t, &sb, sizeof (sb));
  if (ret < 0)
    return ret;
  return iso_write(t, buffer, 512);
}

static
int write_one_dir(Ecma119Image *t, HFSPlusNode *dir)
{
    int ret;
    uint8_t *buffer = NULL;
    size_t i;
    size_t fi_len, len;

    /* buf will point to current write position on buffer */
    uint8_t *buf;

    /* initialize buffer with 0s */
    LIBISO_ALLOC_MEM(buffer, uint8_t, HFSPLUS_BLOCK_SIZE);
    buf = buffer;

    /* write the "." and ".." entries first */
    write_one_dir_record(t, dir, 0, buf, 1, 0);
    buf += 34;
    write_one_dir_record(t, dir, 1, buf, 1, 0);
    buf += 34;

    for (i = 0; i < dir->info.dir->nchildren; i++) {
        int section, nsections;
        HFSPlusNode *child = dir->info.dir->children[i];

        /* compute len of directory entry */
        fi_len = ucslen(child->name) * 2;
        len = fi_len + 34;
        if (child->type == HFSPLUS_FILE && !(t->omit_version_numbers & 3)) {
            len += 4;
        }

        nsections = (child->type == HFSPLUS_FILE) ? child->info.file->nsections : 1;

        for (section = 0; section < nsections; ++section) {

            if ( (buf + len - buffer) > HFSPLUS_BLOCK_SIZE) {
                /* dir doesn't fit in current block */
                ret = iso_write(t, buffer, HFSPLUS_BLOCK_SIZE);
                if (ret < 0) {
                    goto ex;
                }
                memset(buffer, 0, HFSPLUS_BLOCK_SIZE);
                buf = buffer;
            }
            /* write the directory entry in any case */
            write_one_dir_record(t, child, -1, buf, fi_len, section);
            buf += len;
        }
    }

    /* write the last block */
    ret = iso_write(t, buffer, HFSPLUS_BLOCK_SIZE);
ex:;
    LIBISO_FREE_MEM(buffer);
    return ret;
}

static
int write_dirs(Ecma119Image *t, HFSPlusNode *root)
{
    int ret;
    size_t i;

    /* write all directory entries for this dir */
    ret = write_one_dir(t, root);
    if (ret < 0) {
        return ret;
    }

    /* recurse */
    for (i = 0; i < root->info.dir->nchildren; i++) {
        HFSPlusNode *child = root->info.dir->children[i];
        if (child->type == HFSPLUS_DIR) {
            ret = write_dirs(t, child);
            if (ret < 0) {
                return ret;
            }
        }
    }
    return ISO_SUCCESS;
}

static
int hfsplus_writer_write_dirs(IsoImageWriter *writer)
{
    int ret;
    Ecma119Image *t;

    t = writer->target;

    /* first of all, we write the directory structure */
    ret = write_dirs(t, t->hfsplus_root);
    if (ret < 0) {
        return ret;
    }

    return ret;
}

static
int hfsplus_writer_write_data(IsoImageWriter *writer)
{
    int ret;

    if (writer == NULL) {
        return ISO_NULL_POINTER;
    }

    ret = hfsplus_writer_write_dirs(writer);
    if (ret < 0)
        return ret;

    return ISO_SUCCESS;
}

static
int hfsplus_writer_free_data(IsoImageWriter *writer)
{
    /* free the Hfsplus tree */
    Ecma119Image *t = writer->target;
    hfsplus_node_free(t->hfsplus_root);
    return ISO_SUCCESS;
}

int hfsplus_writer_create(Ecma119Image *target)
{
    int ret;
    IsoImageWriter *writer;

    writer = malloc(sizeof(IsoImageWriter));
    if (writer == NULL) {
        return ISO_OUT_OF_MEM;
    }

    writer->compute_data_blocks = hfsplus_writer_compute_data_blocks;
    writer->write_vol_desc = hfsplus_writer_write_vol_desc;
    writer->write_data = hfsplus_writer_write_data;
    writer->free_data = hfsplus_writer_free_data;
    writer->data = NULL;
    writer->target = target;

    iso_msg_debug(target->image->id, "Creating low level Hfsplus tree...");
    ret = hfsplus_tree_create(target);
    if (ret < 0) {
        free((char *) writer);
        return ret;
    }

    /* add this writer to image */
    target->writers[target->nwriters++] = writer;
    target->hfsp_nfiles = 0;
    target->hfsp_ndirs = 0;
    target->hfsp_cat_id = 1;


    /* we need the volume descriptor */
    target->curblock++;
    return ISO_SUCCESS;
}
