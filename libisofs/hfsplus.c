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
#include "system_area.h"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HFSPLUS_BLOCK_SIZE BLOCK_SIZE
#define HFSPLUS_CAT_NODE_SIZE (2 * BLOCK_SIZE)

#include <arpa/inet.h>
/* For these prototypes:
uint16_t ntohs(uint16_t netshort);
uint16_t htons(uint16_t hostshort);
*/


static
int filesrc_block_and_size(Ecma119Image *t, IsoFileSrc *src,
                           uint32_t *start_block, uint64_t *total_size)
{
    int i;
    uint32_t pos;

    *start_block = 0;
    *total_size = 0;
    if (src->nsections <= 0)
        return 0;
    pos = *start_block = src->sections[0].block;
    for (i = 0; i < src->nsections; i++) {
        *total_size += src->sections[i].size;
        if (pos != src->sections[i].block) {
            iso_msg_submit(t->image->id, ISO_SECT_SCATTERED, 0,
                      "File sections do not form consequtive array of blocks");
            return ISO_SECT_SCATTERED;
        }
        /* If .size is not aligned to blocks then there is a byte gap.
           No need to trace the exact byte address.
        */
        pos = src->sections[i].block + src->sections[i].size / 2048;
    }
    return 1;
}

static
uint8_t get_class (uint16_t v)
{
  uint16_t s;
  uint8_t high, low;
  s = ntohs (v);
  high = s >> 8;
  low = v & 0xff;
  if (!hfsplus_class_pages[high])
    return 0;
  return hfsplus_class_pages[high][low];
}

static
int set_hfsplus_name(Ecma119Image *t, char *name, HFSPlusNode *node)
{
    int ret;
    uint16_t *ucs_name, *iptr, *optr;
    uint32_t curlen;
    int done;

    if (name == NULL) {
        /* it is not necessarily an error, it can be the root */
        return ISO_SUCCESS;
    }

    ret = str2ucs(t->input_charset, name, &ucs_name);
    if (ret < 0) {
        iso_msg_debug(t->image->id, "Can't convert %s", name);
        return ret;
    }

    curlen = ucslen (ucs_name);
    node->name = malloc ((curlen * HFSPLUS_MAX_DECOMPOSE_LEN + 1)
			 * sizeof (node->name[0]));
    if (!node->name)
      return ISO_OUT_OF_MEM;

    for (iptr = ucs_name, optr = node->name; *iptr; iptr++)
      {
	const uint16_t *dptr;
	uint16_t val = ntohs (*iptr);
	uint8_t high = val >> 8;
	uint8_t low = val & 0xff;

	if (val == ':')
	  {
	    *optr++ = htons ('/');
	    continue;
	  }

	if (val >= 0xac00 && val <= 0xd7a3)
	  {
	    uint16_t s, l, v, t;
	    s = val - 0xac00;
	    l = s / (21 * 28);
	    v = (s % (21 * 28)) / 28;
	    t = s % 28;
	    *optr++ = htons (l + 0x1100);
	    *optr++ = htons (v + 0x1161);
	    if (t)
	      *optr++ = htons (t + 0x11a7);
	    continue;
	  }
	if (!hfsplus_decompose_pages[high])
	  {
	    *optr++ = *iptr;
	    continue;
	  }
	dptr = hfsplus_decompose_pages[high][low];
	if (!dptr[0])
	  {
	    *optr++ = *iptr;
	    continue;
	  }
	for (; *dptr; dptr++)
	  *optr++ = htons (*dptr);
      }
    *optr = 0;

    do
      {
	uint8_t last_class;
	done = 0;
	if (!ucs_name[0])
	  break;
	last_class = get_class (ucs_name[0]);
	for (optr = node->name + 1; *optr; optr++)
	  {
	    uint8_t new_class = get_class (*optr);

	    if (last_class == 0 || new_class == 0
		|| last_class <= new_class)
	      last_class = new_class;
	    else
	      {
		uint16_t t;
		t = *(optr - 1);
		*(optr - 1) = *optr;
		*optr = t;
	      }
	  }
      }
    while (done);

    node->cmp_name = malloc ((ucslen (node->name) + 1) * sizeof (node->cmp_name[0]));
    if (!node->cmp_name)
      return ISO_OUT_OF_MEM;

    for (iptr = node->name, optr = node->cmp_name; *iptr; iptr++)
      {
	uint8_t high = ((uint8_t *) iptr)[0];
	uint8_t low = ((uint8_t *) iptr)[1];
	if (hfsplus_casefold[high] == 0)
	  {
	    *optr++ = *iptr;
	    continue;
	  }
	if (!hfsplus_casefold[hfsplus_casefold[high] + low])
	  continue;
	*optr++ = ntohs (hfsplus_casefold[hfsplus_casefold[high] + low]);
      }
    *optr = 0;

    free (ucs_name);

    node->strlen = ucslen (node->name);
    return ISO_SUCCESS;
}

static
int hfsplus_count_tree(Ecma119Image *t, IsoNode *iso)
{
    if (t == NULL || iso == NULL) {
        return ISO_NULL_POINTER;
    }

    if (iso->hidden & LIBISO_HIDE_ON_HFSPLUS) {
        /* file will be ignored */
        return 0;
    }

    switch (iso->type) {
    case LIBISO_SYMLINK:
    case LIBISO_SPECIAL:
    case LIBISO_FILE:
      t->hfsp_nfiles++;
      return ISO_SUCCESS;
    case LIBISO_DIR:
      t->hfsp_ndirs++;
      {
	IsoNode *pos;
	IsoDir *dir = (IsoDir*)iso;
	pos = dir->children;
	while (pos) {
	  int cret;
	  cret = hfsplus_count_tree(t, pos);
	  if (cret < 0) {
	    /* error */
	    return cret;
	  }
	  pos = pos->next;
	}
      }
      return ISO_SUCCESS;
    case LIBISO_BOOT:
      return ISO_SUCCESS;
    default:
      /* should never happen */
      return ISO_ASSERT_FAILURE;
    }
}

/**
 * Create the low level Hfsplus tree from the high level ISO tree.
 *
 * @return
 *      1 success, 0 file ignored, < 0 error
 */
static
int create_tree(Ecma119Image *t, IsoNode *iso, uint32_t parent_id)
{
    int ret;
    uint32_t cat_id, cleaf;
    int i;

    if (t == NULL || iso == NULL) {
        return ISO_NULL_POINTER;
    }

    if (iso->hidden & LIBISO_HIDE_ON_HFSPLUS) {
        /* file will be ignored */
        return 0;
    }

    if (iso->type != LIBISO_FILE && iso->type != LIBISO_DIR
	&& iso->type != LIBISO_SYMLINK && iso->type != LIBISO_SPECIAL)
      return 0;

    cat_id = t->hfsp_cat_id++;

    for (i = 0; i < ISO_HFSPLUS_BLESS_MAX; i++)
      if (t->hfsplus_blessed[i] == iso)
	t->hfsp_bless_id[i] = cat_id;

    set_hfsplus_name (t, iso->name, &t->hfsp_leafs[t->hfsp_curleaf]);
    t->hfsp_leafs[t->hfsp_curleaf].node = iso;
    t->hfsp_leafs[t->hfsp_curleaf].cat_id = cat_id;
    t->hfsp_leafs[t->hfsp_curleaf].parent_id = parent_id;
    t->hfsp_leafs[t->hfsp_curleaf].unix_type = UNIX_NONE;

    switch (iso->type)
      {
      case LIBISO_SYMLINK:
	{
	  IsoSymlink *sym = (IsoSymlink*) iso;
	  t->hfsp_leafs[t->hfsp_curleaf].type = HFSPLUS_FILE;
	  t->hfsp_leafs[t->hfsp_curleaf].symlink_size = strlen (sym->dest);
	  t->hfsp_leafs[t->hfsp_curleaf].unix_type = UNIX_SYMLINK;
	  t->hfsp_leafs[t->hfsp_curleaf].used_size = t->hfsp_leafs[t->hfsp_curleaf].strlen * 2 + 8 + 2 + sizeof (struct hfsplus_catfile_common) + 2 * sizeof (struct hfsplus_forkdata);
	  break;
	}
      case LIBISO_SPECIAL:
	t->hfsp_leafs[t->hfsp_curleaf].unix_type = UNIX_SPECIAL;
	t->hfsp_leafs[t->hfsp_curleaf].type = HFSPLUS_FILE;
	t->hfsp_leafs[t->hfsp_curleaf].used_size = t->hfsp_leafs[t->hfsp_curleaf].strlen * 2 + 8 + 2 + sizeof (struct hfsplus_catfile_common) + 2 * sizeof (struct hfsplus_forkdata);
	break;

      case LIBISO_FILE:
	{
	  IsoFile *file = (IsoFile*) iso;
	  t->hfsp_leafs[t->hfsp_curleaf].type = HFSPLUS_FILE;
	  ret = iso_file_src_create(t, file, &t->hfsp_leafs[t->hfsp_curleaf].file);
	  if (ret < 0) {
            return ret;
	  }
	  t->hfsp_leafs[t->hfsp_curleaf].used_size = t->hfsp_leafs[t->hfsp_curleaf].strlen * 2 + 8 + 2 + sizeof (struct hfsplus_catfile_common) + 2 * sizeof (struct hfsplus_forkdata);
	}
	break;
      case LIBISO_DIR:
	{
	  t->hfsp_leafs[t->hfsp_curleaf].type = HFSPLUS_DIR;
	  t->hfsp_leafs[t->hfsp_curleaf].used_size = t->hfsp_leafs[t->hfsp_curleaf].strlen * 2 + 8 + 2 + sizeof (struct hfsplus_catfile_common);
	  break;
	}
      default:
	return ISO_ASSERT_FAILURE;
      }
    cleaf = t->hfsp_curleaf;
    t->hfsp_leafs[t->hfsp_curleaf].nchildren = 0;
    t->hfsp_curleaf++;

    t->hfsp_leafs[t->hfsp_curleaf].name = t->hfsp_leafs[t->hfsp_curleaf - 1].name;
    t->hfsp_leafs[t->hfsp_curleaf].cmp_name = NULL;
    t->hfsp_leafs[t->hfsp_curleaf].strlen = t->hfsp_leafs[t->hfsp_curleaf - 1].strlen;
    t->hfsp_leafs[t->hfsp_curleaf].used_size = t->hfsp_leafs[t->hfsp_curleaf].strlen * 2 + 8 + 2 + sizeof (struct hfsplus_catfile_thread);
    t->hfsp_leafs[t->hfsp_curleaf].node = iso;
    t->hfsp_leafs[t->hfsp_curleaf].type = (iso->type == LIBISO_DIR) ? HFSPLUS_DIR_THREAD : HFSPLUS_FILE_THREAD;
    t->hfsp_leafs[t->hfsp_curleaf].file = 0;
    t->hfsp_leafs[t->hfsp_curleaf].cat_id = parent_id;
    t->hfsp_leafs[t->hfsp_curleaf].parent_id = cat_id;
    t->hfsp_leafs[t->hfsp_curleaf].unix_type = UNIX_NONE;
    t->hfsp_curleaf++;

    if (iso->type == LIBISO_DIR)
      {
	IsoNode *pos;
	IsoDir *dir = (IsoDir*)iso;

	pos = dir->children;
	while (pos)
	  {
	    int cret;
	    cret = create_tree(t, pos, cat_id);
	    if (cret < 0)
	      return cret;
	    pos = pos->next;
	    t->hfsp_leafs[cleaf].nchildren++;
	  }
      }
    return ISO_SUCCESS;
}

static int
cmp_node(const void *f1, const void *f2)
{
  HFSPlusNode *f = (HFSPlusNode*) f1;
  HFSPlusNode *g = (HFSPlusNode*) f2;
  const uint16_t empty[1] = {0};
  const uint16_t *a, *b; 
  if (f->parent_id > g->parent_id)
    return +1;
  if (f->parent_id < g->parent_id)
    return -1;
  a = f->cmp_name;
  b = g->cmp_name;
  if (!a)
    a = empty;
  if (!b)
    b = empty;

  return ucscmp(a, b);
}

static
int hfsplus_tail_writer_compute_data_blocks(IsoImageWriter *writer)
{
  Ecma119Image *t;
  uint32_t hfsp_size;
    
  if (writer == NULL) {
    return ISO_OUT_OF_MEM;
  }

  t = writer->target;

  hfsp_size = t->curblock - t->hfsp_part_start + 1;

  /* We need one bit for every block. */
  /* So if we allocate x blocks we have to satisfy:
     8 * HFSPLUS_BLOCK_SIZE * x >= total_size + x
     (8 * HFSPLUS_BLOCK_SIZE - 1) * x >= total_size
  */
  t->hfsp_allocation_blocks = hfsp_size
    / (8 * HFSPLUS_BLOCK_SIZE - 1) + 1;
  t->hfsp_allocation_file_start = t->curblock;
  t->curblock += t->hfsp_allocation_blocks;
  t->curblock++;
  t->hfsp_total_blocks = t->curblock - t->hfsp_part_start;

  t->apm_block_size = 0x800;
  return iso_quick_apm_entry(t, t->hfsp_part_start, t->hfsp_total_blocks,
                            "HFSPLUS_Hybrid", "Apple_HFS");
}

static
int hfsplus_writer_compute_data_blocks(IsoImageWriter *writer)
{
    Ecma119Image *t;
    uint32_t i;

    if (writer == NULL) {
        return ISO_OUT_OF_MEM;
    }

    t = writer->target;

    iso_msg_debug(t->image->id, "(b) curblock=%d, nodes =%d", t->curblock, t->hfsp_nnodes);
    t->hfsp_part_start = t->curblock;
    t->curblock++;

    t->hfsp_catalog_file_start = t->curblock;
    t->curblock += 2 * t->hfsp_nnodes;

    t->hfsp_extent_file_start = t->curblock;
    t->curblock++;

    iso_msg_debug(t->image->id, "(d) curblock=%d, nodes =%d", t->curblock, t->hfsp_nnodes);
    for (i = 0; i < t->hfsp_nleafs; i++)
      if (t->hfsp_leafs[i].unix_type == UNIX_SYMLINK)
	{
	  t->hfsp_leafs[i].symlink_block = t->curblock;
	  t->curblock += (t->hfsp_leafs[i].symlink_size + HFSPLUS_BLOCK_SIZE - 1) / HFSPLUS_BLOCK_SIZE;
	}

    iso_msg_debug(t->image->id, "(a) curblock=%d, nodes =%d", t->curblock, t->hfsp_nnodes);

    return ISO_SUCCESS;
}

static void set_time (uint32_t *tm, uint32_t t)
{
  iso_msb ((uint8_t *) tm, t + 2082844800, 4);
}

int nop_writer_write_vol_desc(IsoImageWriter *writer)
{
  return ISO_SUCCESS;
}

static
uid_t px_get_uid(Ecma119Image *t, IsoNode *n)
{
    if (t->replace_uid) {
        return t->uid;
    } else {
        return n->uid;
    }
}

static
uid_t px_get_gid(Ecma119Image *t, IsoNode *n)
{
    if (t->replace_gid) {
        return t->gid;
    } else {
        return n->gid;
    }
}

static
mode_t px_get_mode(Ecma119Image *t, IsoNode *n, int isdir)
{
    if (isdir) {
        if (t->replace_dir_mode) {
            return (n->mode & S_IFMT) | t->dir_mode;
        }
    } else {
        if (t->replace_file_mode) {
            return (n->mode & S_IFMT) | t->file_mode;
        }
    }
    return n->mode;
}

int
write_sb (Ecma119Image *t)
{
    struct hfsplus_volheader sb;
    static char buffer[1024];
    int ret;
    int i;

    iso_msg_debug(t->image->id, "Write HFS+ superblock");

    memset (buffer, 0, sizeof (buffer));
    ret = iso_write(t, buffer, 1024);
    if (ret < 0)
      return ret;

    memset (&sb, 0, sizeof (sb));

    t->hfsp_allocation_size = (t->hfsp_total_blocks + 7) >> 3;

    iso_msb ((uint8_t *) &sb.magic, 0x482b, 2);
    iso_msb ((uint8_t *) &sb.version, 4, 2);
    /* Cleanly unmounted, software locked.  */
    iso_msb ((uint8_t *) &sb.attributes, (1 << 8) | (1 << 15), 4);
    iso_msb ((uint8_t *) &sb.last_mounted_version, 0x6c69736f, 4);
    set_time (&sb.ctime, t->now);
    set_time (&sb.utime, t->now);
    set_time (&sb.fsck_time, t->now);
    iso_msb ((uint8_t *) &sb.file_count, t->hfsp_nfiles, 4);
    iso_msb ((uint8_t *) &sb.folder_count, t->hfsp_ndirs - 1, 4);
    iso_msb ((uint8_t *) &sb.blksize, 0x800, 4);
    iso_msb ((uint8_t *) &sb.catalog_node_id, t->hfsp_cat_id, 4);
    iso_msb ((uint8_t *) &sb.rsrc_clumpsize, HFSPLUS_BLOCK_SIZE, 4);
    iso_msb ((uint8_t *) &sb.data_clumpsize, HFSPLUS_BLOCK_SIZE, 4);
    iso_msb ((uint8_t *) &sb.total_blocks, t->hfsp_total_blocks, 4);
    iso_msb ((uint8_t *) &sb.encodings_bitmap + 4, 1, 4);

    iso_msb ((uint8_t *) &sb.allocations_file.size + 4, t->hfsp_allocation_size, 4);
    iso_msb ((uint8_t *) &sb.allocations_file.clumpsize, HFSPLUS_BLOCK_SIZE, 4);
    iso_msb ((uint8_t *) &sb.allocations_file.blocks, (t->hfsp_allocation_size + HFSPLUS_BLOCK_SIZE - 1) / HFSPLUS_BLOCK_SIZE, 4);
    iso_msb ((uint8_t *) &sb.allocations_file.extents[0].start, t->hfsp_allocation_file_start - t->hfsp_part_start, 4);
    iso_msb ((uint8_t *) &sb.allocations_file.extents[0].count, (t->hfsp_allocation_size + HFSPLUS_BLOCK_SIZE - 1) / HFSPLUS_BLOCK_SIZE, 4);

    iso_msb ((uint8_t *) &sb.extents_file.size + 4, HFSPLUS_BLOCK_SIZE, 4);
    iso_msb ((uint8_t *) &sb.extents_file.clumpsize, HFSPLUS_BLOCK_SIZE, 4);
    iso_msb ((uint8_t *) &sb.extents_file.blocks, 1, 4);
    iso_msb ((uint8_t *) &sb.extents_file.extents[0].start, t->hfsp_extent_file_start - t->hfsp_part_start, 4);
    iso_msb ((uint8_t *) &sb.extents_file.extents[0].count, 1, 4);
    iso_msg_debug(t->image->id, "extent_file_start = %d\n", (int)t->hfsp_extent_file_start);

    iso_msb ((uint8_t *) &sb.catalog_file.size + 4, HFSPLUS_BLOCK_SIZE * 2 * t->hfsp_nnodes, 4);
    iso_msb ((uint8_t *) &sb.catalog_file.clumpsize, HFSPLUS_BLOCK_SIZE * 2, 4);
    iso_msb ((uint8_t *) &sb.catalog_file.blocks, 2 * t->hfsp_nnodes, 4);
    iso_msb ((uint8_t *) &sb.catalog_file.extents[0].start, t->hfsp_catalog_file_start - t->hfsp_part_start, 4);
    iso_msb ((uint8_t *) &sb.catalog_file.extents[0].count, 2 * t->hfsp_nnodes, 4);
    iso_msg_debug(t->image->id, "catalog_file_start = %d\n", (int)t->hfsp_catalog_file_start);

    for (i = 0; i < ISO_HFSPLUS_BLESS_MAX; i++)
     iso_msb ((uint8_t *) (&sb.ppc_bootdir + i
			   + (i == ISO_HFSPLUS_BLESS_OSX_FOLDER)),
	      t->hfsp_bless_id[i], 4);

    memcpy (&sb.num_serial, &t->hfsp_serial_number, 8);

    ret = iso_write(t, &sb, sizeof (sb));
    if (ret < 0)
      return ret;
    return iso_write(t, buffer, 512);
}

static
int hfsplus_writer_write_data(IsoImageWriter *writer)
{
    int ret;
    static char buffer[2 * HFSPLUS_BLOCK_SIZE];
    Ecma119Image *t;
    struct hfsplus_btnode *node_head;
    struct hfsplus_btheader *tree_head;
    int level;
    uint32_t curpos = 1, i;

    if (writer == NULL) {
        return ISO_NULL_POINTER;
    }

    t = writer->target;

    iso_msg_debug(t->image->id, "(b) %d written", (int) t->bytes_written / 0x800);

    ret = write_sb (t);
    if (ret < 0)
      return ret;

    iso_msg_debug(t->image->id, "(c) %d written", (int) t->bytes_written / 0x800);

    iso_msg_debug(t->image->id, "real catalog_file_start = %d\n", (int)t->bytes_written / 2048);

    memset (buffer, 0, sizeof (buffer));
    node_head = (struct hfsplus_btnode *) buffer;
    node_head->type = 1;
    iso_msb ((uint8_t *) &node_head->count, 3, 2);
    tree_head = (struct hfsplus_btheader *) (node_head + 1);
    iso_msb ((uint8_t *) &tree_head->depth, t->hfsp_nlevels, 2);
    iso_msb ((uint8_t *) &tree_head->root, 1, 4);
    iso_msb ((uint8_t *) &tree_head->leaf_records, t->hfsp_nleafs, 4);
    iso_msb ((uint8_t *) &tree_head->first_leaf_node, t->hfsp_nnodes - t->hfsp_levels[0].level_size, 4);
    iso_msb ((uint8_t *) &tree_head->last_leaf_node, t->hfsp_nnodes - 1, 4);
    iso_msb ((uint8_t *) &tree_head->nodesize, HFSPLUS_CAT_NODE_SIZE, 2);
    iso_msb ((uint8_t *) &tree_head->keysize, 6 + 2 * LIBISO_HFSPLUS_NAME_MAX, 2);
    iso_msb ((uint8_t *) &tree_head->total_nodes, t->hfsp_nnodes, 4);
    iso_msb ((uint8_t *) &tree_head->free_nodes, 0, 4);
    iso_msb ((uint8_t *) &tree_head->clump_size, HFSPLUS_CAT_NODE_SIZE, 4);
    tree_head->key_compare = 0xcf;
    iso_msb ((uint8_t *) &tree_head->attributes, 2 | 4, 4);
    memset (buffer + 0xf8, -1, t->hfsp_nnodes / 8);
    buffer[0xf8 + (t->hfsp_nnodes / 8)] = 0xff00 >> (t->hfsp_nnodes % 8);

    buffer[HFSPLUS_CAT_NODE_SIZE - 1] = sizeof (*node_head);
    buffer[HFSPLUS_CAT_NODE_SIZE - 3] = sizeof (*node_head) + sizeof (*tree_head);
    buffer[HFSPLUS_CAT_NODE_SIZE - 5] = (char) 0xf8;
    buffer[HFSPLUS_CAT_NODE_SIZE - 7] = (char) ((HFSPLUS_CAT_NODE_SIZE - 8) & 0xff);
    buffer[HFSPLUS_CAT_NODE_SIZE - 8] = (HFSPLUS_CAT_NODE_SIZE - 8) >> 8;

    iso_msg_debug(t->image->id, "Write\n");
    ret = iso_write(t, buffer, HFSPLUS_CAT_NODE_SIZE);
    if (ret < 0)
        return ret;

    for (level = t->hfsp_nlevels - 1; level > 0; level--)
      {
	uint32_t i;
	uint32_t next_lev = curpos + t->hfsp_levels[level].level_size;
	for (i = 0; i < t->hfsp_levels[level].level_size; i++)
	  {
	    uint32_t curoff;
	    uint32_t j;
	    uint32_t curnode = t->hfsp_levels[level].nodes[i].start;
	    memset (buffer, 0, sizeof (buffer));
	    node_head = (struct hfsplus_btnode *) buffer;
	    if (i != t->hfsp_levels[level].level_size - 1)
	      iso_msb ((uint8_t *) &node_head->next, curpos + i + 1, 4);
	    if (i != 0)
	      iso_msb ((uint8_t *) &node_head->prev, curpos + i - 1, 4);
	    node_head->type = 0;
	    node_head->height = level + 1;
	    iso_msb ((uint8_t *) &node_head->count, t->hfsp_levels[level].nodes[i].cnt, 2);
	    curoff = sizeof (struct hfsplus_btnode);
	    for (j = 0; j < t->hfsp_levels[level].nodes[i].cnt; j++)
	      {
		iso_msb ((uint8_t *) buffer + HFSPLUS_CAT_NODE_SIZE - j * 2 - 2, curoff, 2);

		iso_msb ((uint8_t *) buffer + curoff, 2 * t->hfsp_levels[level - 1].nodes[curnode].strlen + 6, 2);
		iso_msb ((uint8_t *) buffer + curoff + 2, t->hfsp_levels[level - 1].nodes[curnode].parent_id, 4);
		iso_msb ((uint8_t *) buffer + curoff + 6, t->hfsp_levels[level - 1].nodes[curnode].strlen, 2);
		curoff += 8;
		memcpy ((uint8_t *) buffer + curoff, t->hfsp_levels[level - 1].nodes[curnode].str, 2 * t->hfsp_levels[level - 1].nodes[curnode].strlen);
		curoff += 2 * t->hfsp_levels[level - 1].nodes[curnode].strlen;
		iso_msb ((uint8_t *) buffer + curoff, next_lev + curnode, 4);
		curoff += 4;
		curnode++;
	      }
	    iso_msb ((uint8_t *) buffer +  HFSPLUS_CAT_NODE_SIZE - j * 2 - 2, curoff, 2);
	    iso_msg_debug(t->image->id, "Write\n");

	    ret = iso_write(t, buffer, HFSPLUS_CAT_NODE_SIZE);

	    if (ret < 0)
	      return ret;
	  }
	curpos = next_lev;
      }

    {
      uint32_t i;
      uint32_t next_lev = curpos + t->hfsp_levels[level].level_size;
      for (i = 0; i < t->hfsp_levels[level].level_size; i++)
	{
	  uint32_t curoff;
	  uint32_t j;
	  uint32_t curnode = t->hfsp_levels[level].nodes[i].start;
	  memset (buffer, 0, sizeof (buffer));
	  node_head = (struct hfsplus_btnode *) buffer;
	  if (i != t->hfsp_levels[level].level_size - 1)
	    iso_msb ((uint8_t *) &node_head->next, curpos + i + 1, 4);
	  if (i != 0)
	    iso_msb ((uint8_t *) &node_head->prev, curpos + i - 1, 4);
	  node_head->type = -1;
	  node_head->height = level + 1;
	  iso_msb ((uint8_t *) &node_head->count, t->hfsp_levels[level].nodes[i].cnt, 2);
	  curoff = sizeof (struct hfsplus_btnode);
	  for (j = 0; j < t->hfsp_levels[level].nodes[i].cnt; j++)
	    {
	      iso_msb ((uint8_t *) buffer + HFSPLUS_CAT_NODE_SIZE - j * 2 - 2, curoff, 2);
	      if (t->hfsp_leafs[curnode].node->name == NULL)
		{
		  iso_msg_debug(t->image->id, "%d out of %d",
				(int) curnode, t->hfsp_nleafs);
		}
	      else
		{
		  iso_msg_debug(t->image->id, "%d out of %d, %s",
				(int) curnode, t->hfsp_nleafs,
				t->hfsp_leafs[curnode].node->name);
		}

	      switch (t->hfsp_leafs[curnode].type)
		{
		case HFSPLUS_FILE_THREAD:
		case HFSPLUS_DIR_THREAD:
		  {
		    struct hfsplus_catfile_thread *thread;
		    iso_msb ((uint8_t *) buffer + curoff, 6, 2);
		    iso_msb ((uint8_t *) buffer + curoff + 2, t->hfsp_leafs[curnode].parent_id, 4);
		    iso_msb ((uint8_t *) buffer + curoff + 6, 0, 2);
		    curoff += 8;
		    thread = (struct hfsplus_catfile_thread *) (buffer + curoff);
		    ((uint8_t *) &thread->type)[1] = t->hfsp_leafs[curnode].type;
		    iso_msb ((uint8_t *) &thread->parentid, t->hfsp_leafs[curnode].cat_id, 4);
		    iso_msb ((uint8_t *) &thread->namelen, t->hfsp_leafs[curnode].strlen, 2);
		    curoff += sizeof (*thread);
		    memcpy (buffer + curoff, t->hfsp_leafs[curnode].name, t->hfsp_leafs[curnode].strlen * 2);
		    curoff += t->hfsp_leafs[curnode].strlen * 2;
		    break;
		  }
		case HFSPLUS_FILE:
		case HFSPLUS_DIR:
		  {
		    struct hfsplus_catfile_common *common;
		    struct hfsplus_forkdata *data_fork;
		    iso_msb ((uint8_t *) buffer + curoff, 6 + 2 * t->hfsp_leafs[curnode].strlen, 2);
		    iso_msb ((uint8_t *) buffer + curoff + 2, t->hfsp_leafs[curnode].parent_id, 4);
		    iso_msb ((uint8_t *) buffer + curoff + 6, t->hfsp_leafs[curnode].strlen, 2);
		    curoff += 8;
		    memcpy (buffer + curoff, t->hfsp_leafs[curnode].name, t->hfsp_leafs[curnode].strlen * 2);
		    curoff += t->hfsp_leafs[curnode].strlen * 2;

		    common = (struct hfsplus_catfile_common *) (buffer + curoff);
		    ((uint8_t *) &common->type)[1] = t->hfsp_leafs[curnode].type;
		    iso_msb ((uint8_t *) &common->valence, t->hfsp_leafs[curnode].nchildren, 4);
		    iso_msb ((uint8_t *) &common->fileid, t->hfsp_leafs[curnode].cat_id, 4);
		    set_time (&common->ctime, t->hfsp_leafs[curnode].node->ctime);
		    set_time (&common->mtime, t->hfsp_leafs[curnode].node->mtime);
		    /* FIXME: distinguish attr_mtime and mtime.  */
		    set_time (&common->attr_mtime, t->hfsp_leafs[curnode].node->mtime);
		    set_time (&common->atime, t->hfsp_leafs[curnode].node->atime);

		    iso_msb ((uint8_t *) &common->uid, px_get_uid (t, t->hfsp_leafs[curnode].node), 4);
		    iso_msb ((uint8_t *) &common->gid, px_get_gid (t, t->hfsp_leafs[curnode].node), 4);
		    iso_msb ((uint8_t *) &common->mode, px_get_mode (t, t->hfsp_leafs[curnode].node, (t->hfsp_leafs[curnode].type == HFSPLUS_DIR)), 2);

		    /*
		      FIXME:
		      uint8_t user_flags;
		      uint8_t group_flags;

		      finder info
		    */
		    if (t->hfsp_leafs[curnode].type == HFSPLUS_FILE)
		      {
			if (t->hfsp_leafs[curnode].unix_type == UNIX_SYMLINK)
			  {
			    memcpy (common->file_type, "slnk", 4);
			    memcpy (common->file_creator, "rhap", 4);
			  }
			else
			  {
			    struct iso_hfsplus_xinfo_data *xinfo;
			    ret = iso_node_get_xinfo(t->hfsp_leafs[curnode].node,
						     iso_hfsplus_xinfo_func,
						     (void *) &xinfo);
			    if (ret > 0)
			      {
				memcpy (common->file_type, xinfo->type_code,
					4);
				memcpy (common->file_creator,
					xinfo->creator_code, 4);
			      }
			    else if (ret < 0)
			      return ret;
			    else 
			      {
				memcpy (common->file_type, "????", 4);
				memcpy (common->file_creator, "????", 4);
			      }
			  }

			if (t->hfsp_leafs[curnode].unix_type == UNIX_SPECIAL
			    && (S_ISBLK(t->hfsp_leafs[curnode].node->mode)
				|| S_ISCHR(t->hfsp_leafs[curnode].node->mode)))
			  iso_msb ((uint8_t *) &common->special,
				   (((IsoSpecial*) t->hfsp_leafs[curnode].node)->dev & 0xffffffff),
				   4);

			iso_msb ((uint8_t *) &common->flags, 2, 2);
		      }
		    else if (t->hfsp_leafs[curnode].type == HFSPLUS_DIR)
		      {
			iso_msb ((uint8_t *) &common->flags, 0, 2);
		      }
		    curoff += sizeof (*common);
		    if (t->hfsp_leafs[curnode].type == HFSPLUS_FILE)
		      {
			uint64_t sz;
			uint32_t blk;
			data_fork = (struct hfsplus_forkdata *) (buffer + curoff);

			if (t->hfsp_leafs[curnode].unix_type == UNIX_SYMLINK)
			  {
			    blk = t->hfsp_leafs[curnode].symlink_block;
			    sz = t->hfsp_leafs[curnode].symlink_size;
			  }
			else if (t->hfsp_leafs[curnode].unix_type == UNIX_SPECIAL)
			  {
			    blk = 0;
			    sz = 0;
			  }
			else
			  {
			    ret = filesrc_block_and_size(t,
							 t->hfsp_leafs[curnode].file,
							 &blk, &sz);
			    if (ret <= 0)
			      return ret;
			  }
			if (sz == 0)
			  blk = t->hfsp_part_start;
			iso_msb ((uint8_t *) &data_fork->size, sz >> 32, 4);
			iso_msb ((uint8_t *) &data_fork->size + 4, sz, 4);
			iso_msb ((uint8_t *) &data_fork->clumpsize, HFSPLUS_BLOCK_SIZE, 4);
			iso_msb ((uint8_t *) &data_fork->blocks, (sz + HFSPLUS_BLOCK_SIZE - 1) / HFSPLUS_BLOCK_SIZE, 4);
			iso_msb ((uint8_t *) &data_fork->extents[0].start, blk - t->hfsp_part_start, 4);
			iso_msb ((uint8_t *) &data_fork->extents[0].count, (sz + HFSPLUS_BLOCK_SIZE - 1) / HFSPLUS_BLOCK_SIZE, 4);
			
			curoff += sizeof (*data_fork) * 2;
			/* FIXME: resource fork */
		      }
		    break;
		  }
		}
	      curnode++;
	    }
	  iso_msb ((uint8_t *) buffer + HFSPLUS_CAT_NODE_SIZE - j * 2 - 2, curoff, 2);
	  iso_msg_debug(t->image->id, "Write\n");
	  ret = iso_write(t, buffer, HFSPLUS_CAT_NODE_SIZE);
	  if (ret < 0)
	    return ret;
	}
	curpos = next_lev;
    }

    iso_msg_debug(t->image->id, "real extent_file_start = %d\n", (int)t->bytes_written / 2048);

    memset (buffer, 0, sizeof (buffer));
    node_head = (struct hfsplus_btnode *) buffer;
    node_head->type = 1;
    iso_msb ((uint8_t *) &node_head->count, 3, 2);
    tree_head = (struct hfsplus_btheader *) (node_head + 1);
    iso_msb ((uint8_t *) &tree_head->nodesize, HFSPLUS_BLOCK_SIZE, 2);
    iso_msb ((uint8_t *) &tree_head->keysize, 10, 2);
    iso_msb ((uint8_t *) &tree_head->total_nodes, 1, 4);
    iso_msb ((uint8_t *) &tree_head->free_nodes, 0, 4);
    iso_msb ((uint8_t *) &tree_head->clump_size, HFSPLUS_BLOCK_SIZE, 4);
    iso_msb ((uint8_t *) &tree_head->attributes, 2, 4);
    buffer[0xf8] = (char) 0x80;

    buffer[HFSPLUS_BLOCK_SIZE - 1] = sizeof (*node_head);
    buffer[HFSPLUS_BLOCK_SIZE - 3] = sizeof (*node_head) + sizeof (*tree_head);
    buffer[HFSPLUS_BLOCK_SIZE - 5] = (char) 0xf8;
    buffer[HFSPLUS_BLOCK_SIZE - 7] = (char) ((HFSPLUS_BLOCK_SIZE - 8) & 0xff);
    buffer[HFSPLUS_BLOCK_SIZE - 8] = (HFSPLUS_BLOCK_SIZE - 8) >> 8;

    ret = iso_write(t, buffer, HFSPLUS_BLOCK_SIZE);
    if (ret < 0)
        return ret;

    iso_msg_debug(t->image->id, "(d) %d written", (int) t->bytes_written / 0x800);
    memset (buffer, 0, sizeof (buffer));
    for (i = 0; i < t->hfsp_nleafs; i++)
      if (t->hfsp_leafs[i].unix_type == UNIX_SYMLINK)
	{
	  IsoSymlink *sym = (IsoSymlink*) t->hfsp_leafs[i].node;
	  int overhead;
	  ret = iso_write(t, sym->dest, t->hfsp_leafs[i].symlink_size);
	  if (ret < 0)
	    return ret;
	  overhead = t->hfsp_leafs[i].symlink_size % HFSPLUS_BLOCK_SIZE;
	  if (overhead)
	    overhead = HFSPLUS_BLOCK_SIZE - overhead;
	  ret = iso_write(t, buffer, overhead);
	  if (ret < 0)
	    return ret;
	}

    iso_msg_debug(t->image->id, "(a) %d written", (int) t->bytes_written / 0x800);
    return ISO_SUCCESS;
}

static
int hfsplus_tail_writer_write_data(IsoImageWriter *writer)
{
    int ret;
    static char buffer[2 * HFSPLUS_BLOCK_SIZE];
    uint32_t complete_blocks, remaining_blocks;
    int over;
    Ecma119Image *t;

    if (writer == NULL) {
        return ISO_NULL_POINTER;
    }

    t = writer->target;

    memset (buffer, -1, sizeof (buffer));
    complete_blocks = (t->hfsp_allocation_size - 1) / HFSPLUS_BLOCK_SIZE;
    remaining_blocks = t->hfsp_allocation_blocks - complete_blocks;

    while (complete_blocks--)
      {
	ret = iso_write(t, buffer, HFSPLUS_BLOCK_SIZE);
	if (ret < 0)
	  return ret;
      }
    over = (t->hfsp_allocation_size - 1) % HFSPLUS_BLOCK_SIZE;
    if (over)
      {
	memset (buffer + over, 0, sizeof (buffer) - over);
	buffer[over] = 0xff00 >> (t->hfsp_total_blocks % 8);
	ret = iso_write(t, buffer, HFSPLUS_BLOCK_SIZE);
	if (ret < 0)
	  return ret;
	remaining_blocks--;
      }
    memset (buffer, 0, sizeof (buffer));
    /* When we have both FAT and HFS+ we may to overestimate needed blocks a bit.  */
    while (remaining_blocks--)
      {
	ret = iso_write(t, buffer, HFSPLUS_BLOCK_SIZE);
	if (ret < 0)
	  return ret;
      }

    iso_msg_debug(t->image->id, "%d written", (int) t->bytes_written);

    return write_sb (t);
}

static
int hfsplus_writer_free_data(IsoImageWriter *writer)
{
    /* free the Hfsplus tree */
    Ecma119Image *t = writer->target;
    uint32_t i;
    for (i = 0; i < t->hfsp_curleaf; i++)
      if (t->hfsp_leafs[i].type != HFSPLUS_FILE_THREAD
	  && t->hfsp_leafs[i].type != HFSPLUS_DIR_THREAD)
	{
	  free (t->hfsp_leafs[i].name);
	  free (t->hfsp_leafs[i].cmp_name);
	}
    free(t->hfsp_leafs);
    for (i = 0; i < t->hfsp_nlevels; i++)
      free (t->hfsp_levels[i].nodes);
    free(t->hfsp_levels);
    return ISO_SUCCESS;
}

static
int nop_writer_free_data(IsoImageWriter *writer)
{
    return ISO_SUCCESS;
}

int hfsplus_writer_create(Ecma119Image *target)
{
    int ret;
    IsoImageWriter *writer;
    int max_levels;
    int level = 0;
    IsoNode *pos;
    IsoDir *dir;
    int i;

    writer = malloc(sizeof(IsoImageWriter));
    if (writer == NULL) {
        return ISO_OUT_OF_MEM;
    }

    make_hfsplus_decompose_pages();
    make_hfsplus_class_pages();

    writer->compute_data_blocks = hfsplus_writer_compute_data_blocks;
    writer->write_vol_desc = nop_writer_write_vol_desc;
    writer->write_data = hfsplus_writer_write_data;
    writer->free_data = hfsplus_writer_free_data;
    writer->data = NULL;
    writer->target = target;

    iso_msg_debug(target->image->id, "Creating low level Hfsplus tree...");
    target->hfsp_nfiles = 0;
    target->hfsp_ndirs = 0;
    target->hfsp_cat_id = 16;
    ret = hfsplus_count_tree(target, (IsoNode*)target->image->root);
    if (ret < 0) {
        free((char *) writer);
        return ret;
    }

    for (i = 0; i < ISO_HFSPLUS_BLESS_MAX; i++)
      target->hfsp_bless_id[i] = 0;

    target->hfsp_nleafs = 2 * (target->hfsp_nfiles + target->hfsp_ndirs);
    target->hfsp_curleaf = 0;

    target->hfsp_leafs = malloc (target->hfsp_nleafs * sizeof (target->hfsp_leafs[0]));
    if (target->hfsp_leafs == NULL) {
        return ISO_OUT_OF_MEM;
    }

    set_hfsplus_name (target, target->image->volume_id, &target->hfsp_leafs[target->hfsp_curleaf]);
    target->hfsp_leafs[target->hfsp_curleaf].node = (IsoNode *) target->image->root;
    target->hfsp_leafs[target->hfsp_curleaf].used_size = target->hfsp_leafs[target->hfsp_curleaf].strlen * 2 + 8 + 2 + sizeof (struct hfsplus_catfile_common);

    target->hfsp_leafs[target->hfsp_curleaf].type = HFSPLUS_DIR;
    target->hfsp_leafs[target->hfsp_curleaf].file = 0;
    target->hfsp_leafs[target->hfsp_curleaf].cat_id = 2;
    target->hfsp_leafs[target->hfsp_curleaf].parent_id = 1;
    target->hfsp_leafs[target->hfsp_curleaf].nchildren = 0;
    target->hfsp_leafs[target->hfsp_curleaf].unix_type = UNIX_NONE;
    target->hfsp_curleaf++;

    target->hfsp_leafs[target->hfsp_curleaf].name = target->hfsp_leafs[target->hfsp_curleaf - 1].name;
    target->hfsp_leafs[target->hfsp_curleaf].cmp_name = 0;
    target->hfsp_leafs[target->hfsp_curleaf].strlen = target->hfsp_leafs[target->hfsp_curleaf - 1].strlen;
    target->hfsp_leafs[target->hfsp_curleaf].used_size = target->hfsp_leafs[target->hfsp_curleaf].strlen * 2 + 8 + 2 + sizeof (struct hfsplus_catfile_thread);
    target->hfsp_leafs[target->hfsp_curleaf].node = (IsoNode *) target->image->root;
    target->hfsp_leafs[target->hfsp_curleaf].type = HFSPLUS_DIR_THREAD;
    target->hfsp_leafs[target->hfsp_curleaf].file = 0;
    target->hfsp_leafs[target->hfsp_curleaf].cat_id = 1;
    target->hfsp_leafs[target->hfsp_curleaf].parent_id = 2;
    target->hfsp_leafs[target->hfsp_curleaf].unix_type = UNIX_NONE;
    target->hfsp_curleaf++;

    dir = (IsoDir*)target->image->root;

    pos = dir->children;
    while (pos)
      {
	int cret;
	cret = create_tree(target, pos, 2);
	if (cret < 0)
	  return cret;
	pos = pos->next;
	target->hfsp_leafs[0].nchildren++;
      }

    qsort(target->hfsp_leafs, target->hfsp_nleafs,
          sizeof(*target->hfsp_leafs), cmp_node);

    for (max_levels = 0; target->hfsp_nleafs >> max_levels; max_levels++);
    max_levels += 2;
    target->hfsp_levels = malloc (max_levels * sizeof (target->hfsp_levels[0]));
    if (target->hfsp_levels == NULL) {
        return ISO_OUT_OF_MEM;
    }

    target->hfsp_nnodes = 1;
    {
      uint32_t last_start = 0;
      uint32_t i;
      unsigned bytes_rem = HFSPLUS_CAT_NODE_SIZE - sizeof (struct hfsplus_btnode) - 2;

      target->hfsp_levels[level].nodes = malloc ((target->hfsp_nleafs + 1) *  sizeof (target->hfsp_levels[level].nodes[0]));
      if (!target->hfsp_levels[level].nodes)
	return ISO_OUT_OF_MEM;
    
      target->hfsp_levels[level].level_size = 0;  
      for (i = 0; i < target->hfsp_nleafs; i++)
	{
	  if (bytes_rem < target->hfsp_leafs[i].used_size)
	    {
	      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].start = last_start;
	      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].cnt = i - last_start;
	      if (target->hfsp_leafs[last_start].cmp_name)
		{
		  target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].strlen = target->hfsp_leafs[last_start].strlen;
		  target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].str = target->hfsp_leafs[last_start].name;
		}
	      else
		{
		  target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].strlen = 0;
		  target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].str = NULL;
		}
	      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].parent_id = target->hfsp_leafs[last_start].parent_id;
	      target->hfsp_levels[level].level_size++;
	      last_start = i;
	      bytes_rem = HFSPLUS_CAT_NODE_SIZE - sizeof (struct hfsplus_btnode) - 2;
	    }
	  bytes_rem -= target->hfsp_leafs[i].used_size;
	}

      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].start = last_start;
      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].cnt = i - last_start;
      if (target->hfsp_leafs[last_start].cmp_name)
	{
	  target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].strlen = target->hfsp_leafs[last_start].strlen;
	  target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].str = target->hfsp_leafs[last_start].name;
	}
      else
	{
	  target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].strlen = 0;
	  target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].str = NULL;
	}
      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].parent_id = target->hfsp_leafs[last_start].parent_id;
      target->hfsp_levels[level].level_size++;
      target->hfsp_nnodes += target->hfsp_levels[level].level_size;
    }

    while (target->hfsp_levels[level].level_size > 1)
      {
	uint32_t last_start = 0;
	uint32_t i;
	uint32_t last_size;
	unsigned bytes_rem = HFSPLUS_CAT_NODE_SIZE - sizeof (struct hfsplus_btnode) - 2;

	last_size = target->hfsp_levels[level].level_size;

	level++;

	target->hfsp_levels[level].nodes = malloc (((last_size + 1) / 2) *  sizeof (target->hfsp_levels[level].nodes[0]));
	if (!target->hfsp_levels[level].nodes)
	  return ISO_OUT_OF_MEM;
    
	target->hfsp_levels[level].level_size = 0;  

	for (i = 0; i < last_size; i++)
	  {
	    uint32_t used_size;
	    used_size = target->hfsp_levels[level - 1].nodes[i].strlen * 2 + 14;
	    if (bytes_rem < used_size)
	    {
	      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].start = last_start;
	      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].cnt = i - last_start;
	      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].strlen = target->hfsp_levels[level - 1].nodes[last_start].strlen;
	      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].str = target->hfsp_levels[level - 1].nodes[last_start].str;
	      target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].parent_id = target->hfsp_levels[level - 1].nodes[last_start].parent_id;
	      target->hfsp_levels[level].level_size++;
	      last_start = i;
	      bytes_rem = HFSPLUS_CAT_NODE_SIZE - sizeof (struct hfsplus_btnode) - 2;
	    }
	  bytes_rem -= used_size;
	}

	target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].start = last_start;
	target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].cnt = i - last_start;
	target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].strlen = target->hfsp_levels[level - 1].nodes[last_start].strlen;
	target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].str = target->hfsp_levels[level - 1].nodes[last_start].str;
	target->hfsp_levels[level].nodes[target->hfsp_levels[level].level_size].parent_id = target->hfsp_levels[level - 1].nodes[last_start].parent_id;
	target->hfsp_levels[level].level_size++;
	target->hfsp_nnodes += target->hfsp_levels[level].level_size;
      }

    target->hfsp_nlevels = level + 1;

    if (target->hfsp_nnodes > (HFSPLUS_CAT_NODE_SIZE - 0x100) * 8)
      {
	return iso_msg_submit(target->image->id, ISO_MANGLE_TOO_MUCH_FILES, 0,
			      "HFS+ map nodes aren't implemented");

	return ISO_MANGLE_TOO_MUCH_FILES;
      }

    /* add this writer to image */
    target->writers[target->nwriters++] = writer;

    return ISO_SUCCESS;
}

int hfsplus_tail_writer_create(Ecma119Image *target)
{
    IsoImageWriter *writer;

    writer = malloc(sizeof(IsoImageWriter));
    if (writer == NULL) {
        return ISO_OUT_OF_MEM;
    }

    writer->compute_data_blocks = hfsplus_tail_writer_compute_data_blocks;
    writer->write_vol_desc = nop_writer_write_vol_desc;
    writer->write_data = hfsplus_tail_writer_write_data;
    writer->free_data = nop_writer_free_data;
    writer->data = NULL;
    writer->target = target;

    /* add this writer to image */
    target->writers[target->nwriters++] = writer;

    return ISO_SUCCESS;
}
