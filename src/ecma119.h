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
typedef struct joliet_node JolietNode;
typedef struct Iso_File_Src IsoFileSrc;
typedef struct Iso_Image_Writer IsoImageWriter;

struct ecma119_image
{
    IsoImage *image;
    Ecma119Node *root;

    unsigned int iso_level :2;

    /* extensions */
    unsigned int rockridge :1;
    unsigned int joliet :1;
    unsigned int eltorito :1;

    /* relaxed constraints */
    unsigned int omit_version_numbers :1;
    unsigned int allow_deep_paths :1;
    
    /** Allow paths on Joliet tree to be larger than 240 bytes */
    unsigned int joliet_longer_paths :1;
    //    int relaxed_constraints; /**< see ecma119_relaxed_constraints_flag */

    /* 
     * Mode replace. If one of these flags is set, the correspodent values are
     * replaced with values below.
     */
    unsigned int replace_uid :1;
    unsigned int replace_gid :1;
    unsigned int replace_file_mode :1;
    unsigned int replace_dir_mode :1;

    uid_t uid;
    gid_t gid;
    mode_t file_mode;
    mode_t dir_mode;

    /**
     *  if sort files or not. Sorting is based of the weight of each file 
     */
    int sort_files;

    /**
     * In the CD, each file must have an unique inode number. So each
     * time we add a new file, this is incremented.
     */
    ino_t ino;

    char *input_charset;
    char *output_charset;

    unsigned int appendable : 1;
    uint32_t ms_block; /**< start block for a ms image */
    time_t now; /**< Time at which writing began. */

    /** Total size of the output. This only includes the current volume. */
    off_t total_size;
    uint32_t vol_space_size;

    /* Bytes already written, just for progress notification */
    off_t bytes_written;
    int percent_written;

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
    
    /*
     * Joliet related information
     */
    JolietNode *joliet_root;
    size_t joliet_ndirs;
    uint32_t joliet_path_table_size;
    uint32_t joliet_l_path_table_pos;
    uint32_t joliet_m_path_table_pos;
    
    /*
     * El-Torito related information
     */
    struct el_torito_boot_catalog *catalog;
    IsoFileSrc *cat; /**< location of the boot catalog in the new image */
    uint32_t imgblock; /**< location of the boot image in the new image */

    /*
     * Number of pad blocks that we need to write. Padding blocks are blocks
     * filled by 0s that we put between the directory structures and the file
     * data. These padding blocks are added by libisofs to improve the handling
     * of image growing. The idea is that the first blocks in the image are
     * overwritten with the volume descriptors of the new image. These first
     * blocks usually correspond to the volume descriptors and directory 
     * structure of the old image, and can be safety overwritten. However, 
     * with very small images they might correspond to valid data. To ensure 
     * this never happens, what we do is to add padding bytes, to ensure no 
     * file data is written in the first 64 KiB, that are the bytes we usually
     * overwrite.
     */
    uint32_t pad_blocks;

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

/* ECMA-119, 8.5 */
struct ecma119_sup_vol_desc
{
    uint8_t vol_desc_type            BP(1, 1);
    uint8_t std_identifier           BP(2, 6);
    uint8_t vol_desc_version         BP(7, 7);
    uint8_t vol_flags                BP(8, 8);
    uint8_t system_id                BP(9, 40);
    uint8_t volume_id                BP(41, 72);
    uint8_t unused2                  BP(73, 80);
    uint8_t vol_space_size           BP(81, 88);
    uint8_t esc_sequences            BP(89, 120);
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
