/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_ECMA119_H_
#define LIBISO_ECMA119_H_

#include "libisofs.h"
#include "util.h"
#include "buffer.h"

#include <stdint.h>
#include <pthread.h>

#define BLOCK_SIZE      2048

typedef struct ecma119_image Ecma119Image;
typedef struct ecma119_node Ecma119Node;
typedef struct Iso_File_Src IsoFileSrc;
typedef struct Iso_Image_Writer IsoImageWriter;

struct ecma119_image {
    IsoImage *image;
    Ecma119Node *root;
    
    unsigned int iso_level:2;
    
    /* relaxed constraints */
    unsigned int omit_version_numbers:1;
    
//    int relaxed_constraints; /**< see ecma119_relaxed_constraints_flag */
//    
//    int replace_mode; /**< Replace ownership and modes of files
//                      * 
//                      * 0. filesystem values
//                      * 1. useful values
//                      * bits 1-4 bitmask: 
//                      *     2 - replace dir
//                      *     3 - replace file 
//                      *     4 - replace gid
//                      *     5 - replace uid
//                      */
//    mode_t dir_mode;
//    mode_t file_mode;
//    gid_t gid;
//    uid_t uid;
    int sort_files; /**< if sort files or not. Sorting is based of 
                      *  the weight of each file */
    
    char *input_charset;
    
    uint32_t ms_block; /**< start block for a ms image */
    time_t now;     /**< Time at which writing began. */
    off_t total_size; /**< Total size of the output. This only
                        *  includes the current volume. */
    uint32_t vol_space_size;

    /*
     * Block being processed, either during image writing or structure
     * size calculation.
     */
    uint32_t curblock;

    /* 
     * number of dirs in ECMA-119 tree, computed together with dir position,
     * and needed for path table computation in a efficient way
     */
    size_t ndirs;
    uint32_t path_table_size;
    uint32_t l_path_table_pos;
    uint32_t m_path_table_pos;
    
    size_t nwriters;
    IsoImageWriter **writers;

    /* tree of files sources */
    IsoRBTree *files;
    
    /* Buffer for communication between burn_source and writer thread */
    IsoRingBuffer *buffer;
    
    /* writer thread descriptor */
    pthread_t wthread;
    pthread_attr_t th_attr;
};

#define BP(a,b) [(b) - (a) + 1]

/* ECMA-119, 8.4 */
struct ecma119_pri_vol_desc
{
    uint8_t vol_desc_type            BP(1, 1);
    uint8_t std_identifier           BP(2, 6);
    uint8_t vol_desc_version         BP(7, 7);
    uint8_t unused1                  BP(8, 8);
    uint8_t system_id                BP(9, 40);
    uint8_t volume_id                BP(41, 72);
    uint8_t unused2                  BP(73, 80);
    uint8_t vol_space_size           BP(81, 88);
    uint8_t unused3                  BP(89, 120);
    uint8_t vol_set_size             BP(121, 124);
    uint8_t vol_seq_number           BP(125, 128);
    uint8_t block_size               BP(129, 132);
    uint8_t path_table_size          BP(133, 140);
    uint8_t l_path_table_pos         BP(141, 144);
    uint8_t opt_l_path_table_pos     BP(145, 148);
    uint8_t m_path_table_pos         BP(149, 152);
    uint8_t opt_m_path_table_pos     BP(153, 156);
    uint8_t root_dir_record          BP(157, 190);
    uint8_t vol_set_id               BP(191, 318);
    uint8_t publisher_id             BP(319, 446);
    uint8_t data_prep_id             BP(447, 574);
    uint8_t application_id           BP(575, 702);
    uint8_t copyright_file_id        BP(703, 739);
    uint8_t abstract_file_id         BP(740, 776);
    uint8_t bibliographic_file_id    BP(777, 813);
    uint8_t vol_creation_time        BP(814, 830);
    uint8_t vol_modification_time    BP(831, 847);
    uint8_t vol_expiration_time      BP(848, 864);
    uint8_t vol_effective_time       BP(865, 881);
    uint8_t file_structure_version   BP(882, 882);
    uint8_t reserved1                BP(883, 883);
    uint8_t app_use                  BP(884, 1395);
    uint8_t reserved2                BP(1396, 2048);
};

/* ECMA-119, 9.1 */
struct ecma119_dir_record
{
    uint8_t len_dr                   BP(1, 1);
    uint8_t len_xa                   BP(2, 2);
    uint8_t block                    BP(3, 10);
    uint8_t length                   BP(11, 18);
    uint8_t recording_time           BP(19, 25);
    uint8_t flags                    BP(26, 26);
    uint8_t file_unit_size           BP(27, 27);
    uint8_t interleave_gap_size      BP(28, 28);
    uint8_t vol_seq_number           BP(29, 32);
    uint8_t len_fi                   BP(33, 33);
    uint8_t file_id                  BP(34, 34); /* 34 to 33+len_fi */
    /* padding field (if len_fi is even) */
    /* system use (len_dr - len_su + 1 to len_dr) */
};

/* ECMA-119, 9.4 */
struct ecma119_path_table_record
{
    uint8_t len_di                   BP(1, 1);
    uint8_t len_xa                   BP(2, 2);
    uint8_t block                    BP(3, 6);
    uint8_t parent                   BP(7, 8);
    uint8_t dir_id                   BP(9, 9); /* 9 to 8+len_di */
    /* padding field (if len_di is odd) */
};

/* ECMA-119, 8.3 */
struct ecma119_vol_desc_terminator
{
    uint8_t vol_desc_type            BP(1, 1);
    uint8_t std_identifier           BP(2, 6);
    uint8_t vol_desc_version         BP(7, 7);
    uint8_t reserved                 BP(8, 2048);
};

#endif /*LIBISO_ECMA119_H_*/
