/*
 * Copyright (c) 2012 Thomas Schmitt
 * Copyright (c) 2012 Vladimir Serbinenko
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 */

/**
 * Declare HFS+ related structures.
 */

#ifndef LIBISO_HFSPLUS_H
#define LIBISO_HFSPLUS_H

#include "ecma119.h"



/* <<< dummies */

#define LIBISO_HFSPLUS_NAME_MAX 255

enum hfsplus_node_type {
        HFSPLUS_FILE,
        HFSPLUS_DIR
};

struct hfsplus_dir_info {
    HFSPlusNode **children;
        size_t nchildren;
        size_t len;
        size_t block;
};

struct hfsplus_node
{
    uint16_t *name; /**< Name in UCS-2BE. */

    HFSPlusNode *parent;

    IsoNode *node; /*< reference to the iso node */

    enum hfsplus_node_type type;
    union {
        IsoFileSrc *file;
        struct hfsplus_dir_info *dir;
    } info;

    /* <<< dummies */
    int cat_id;
    
};

struct hfsplus_volheader {

 uint16_t magic;
 uint16_t version;
 uint32_t attributes;
 uint32_t last_mounted_version;
 uint32_t ctime;
 uint32_t utime;
 uint32_t backup_time;
 uint32_t fsck_time;
 uint32_t file_count;
 uint32_t folder_count;
 uint32_t blksize;
 uint32_t catalog_node_id;
 uint32_t rsrc_clumpsize;
 uint32_t data_clumpsize;
 uint32_t total_blocks;
};


/* >>> ts B20523 : what else is needed here ? */




/**
 * Create a IsoWriter to deal with HFS+ structures, and add it to the given
 * target.
 *
 * @return
 *      1 on success, < 0 on error
 */
int hfsplus_writer_create(Ecma119Image *target);


/* Not to be called but only for comparison with target->writers[i]
*/
int hfsplus_writer_write_vol_desc(IsoImageWriter *writer);

#endif /* LIBISO_HFSPLUS_H */
