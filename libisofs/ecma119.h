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

/*
 * Maximum file section size. Set to 4GB - 1 =  0xffffffff
 */
#define MAX_ISO_FILE_SECTION_SIZE   0xffffffff

/*
 * When a file need to be splitted in several sections, the maximum size
 * of such sections, but the last one. Set to a multiple of BLOCK_SIZE.
 * Default to 4GB - 2048 = 0xFFFFF800
 */
#define ISO_EXTENT_SIZE             0xFFFFF800

/**
 * Holds the options for the image generation.
 */
struct iso_write_opts {

    int level; /**< ISO level to write at. (ECMA-119, 10) */

    /** Which extensions to support. */
    unsigned int rockridge :1;
    unsigned int joliet :1;
    unsigned int iso1999 :1;

    /* allways write timestamps in GMT */
    unsigned int always_gmt :1;

    /*
     * Relaxed constraints. Setting any of these to 1 break the specifications,
     * but it is supposed to work on most moderns systems. Use with caution.
     */

    /**
     * Omit the version number (";1") at the end of the ISO-9660 identifiers.
     * Version numbers are usually not used.
     */
    unsigned int omit_version_numbers :1;

    /**
     * Allow ISO-9660 directory hierarchy to be deeper than 8 levels.
     */
    unsigned int allow_deep_paths :1;

    /**
     * Allow path in the ISO-9660 tree to have more than 255 characters.
     */
    unsigned int allow_longer_paths :1;

    /**
     * Allow a single file or directory hierarchy to have up to 37 characters.
     * This is larger than the 31 characters allowed by ISO level 2, and the
     * extra space is taken from the version number, so this also forces
     * omit_version_numbers.
     */
    unsigned int max_37_char_filenames :1;

    /**
     * ISO-9660 forces filenames to have a ".", that separates file name from
     * extension. libisofs adds it if original filename doesn't has one. Set
     * this to 1 to prevent this behavior
     */
    unsigned int no_force_dots :1;

    /**
     * Allow lowercase characters in ISO-9660 filenames. By default, only
     * uppercase characters, numbers and a few other characters are allowed.
     */
    unsigned int allow_lowercase :1;

    /**
     * Allow all ASCII characters to be appear on an ISO-9660 filename. Note
     * that "/" and "\0" characters are never allowed, even in RR names.
     */
    unsigned int allow_full_ascii :1;

    /**
     * Allow all characters to be part of Volume and Volset identifiers on
     * the Primary Volume Descriptor. This breaks ISO-9660 contraints, but
     * should work on modern systems.
     */
    unsigned int relaxed_vol_atts :1;

    /**
     * Allow paths in the Joliet tree to have more than 240 characters.
     */
    unsigned int joliet_longer_paths :1;

    /** If files should be sorted based on their weight. */
    unsigned int sort_files :1;

    /**
     * The following options set the default values for files and directory
     * permissions, gid and uid. All these take one of three values: 0, 1 or 2.
     * If 0, the corresponding attribute will be kept as setted in the IsoNode.
     * Unless you have changed it, it corresponds to the value on disc, so it
     * is suitable for backup purposes. If set to 1, the corresponding attrib.
     * will be changed by a default suitable value. Finally, if you set it to
     * 2, the attrib. will be changed with the value specified in the options
     * below. Note that for mode attributes, only the permissions are set, the
     * file type remains unchanged.
     */
    unsigned int replace_dir_mode :2;
    unsigned int replace_file_mode :2;
    unsigned int replace_uid :2;
    unsigned int replace_gid :2;

    mode_t dir_mode; /** Mode to use on dirs when replace_dir_mode == 2. */
    mode_t file_mode; /** Mode to use on files when replace_file_mode == 2. */
    uid_t uid; /** uid to use when replace_uid == 2. */
    gid_t gid; /** gid to use when replace_gid == 2. */

    /**
     * 0 to use IsoNode timestamps, 1 to use recording time, 2 to use
     * values from timestamp field. This has only meaning if RR extensions
     * are enabled.
     */
    unsigned int replace_timestamps :2;
    time_t timestamp;

    /**
     * Charset for the RR filenames that will be created.
     * NULL to use default charset, the locale one.
     */
    char *output_charset;

    /**
     * This flags control the type of the image to create. Libisofs support
     * two kind of images: stand-alone and appendable.
     *
     * A stand-alone image is an image that is valid alone, and that can be
     * mounted by its own. This is the kind of image you will want to create
     * in most cases. A stand-alone image can be burned in an empty CD or DVD,
     * or write to an .iso file for future burning or distribution.
     *
     * On the other side, an appendable image is not self contained, it refers
     * to serveral files that are stored outside the image. Its usage is for
     * multisession discs, where you add data in a new session, while the
     * previous session data can still be accessed. In those cases, the old
     * data is not written again. Instead, the new image refers to it, and thus
     * it's only valid when appended to the original. Note that in those cases
     * the image will be written after the original, and thus you will want
     * to use a ms_block greater than 0.
     *
     * Note that if you haven't import a previous image (by means of
     * iso_image_import()), the image will always be a stand-alone image, as
     * there is no previous data to refer to.
     */
    unsigned int appendable : 1;

    /**
     * Start block of the image. It is supposed to be the lba where the first
     * block of the image will be written on disc. All references inside the
     * ISO image will take this into account, thus providing a mountable image.
     *
     * For appendable images, that are written to a new session, you should
     * pass here the lba of the next writable address on disc.
     *
     * In stand alone images this is usually 0. However, you may want to
     * provide a different ms_block if you don't plan to burn the image in the
     * first session on disc, such as in some CD-Extra disc whether the data
     * image is written in a new session after some audio tracks.
     */
    uint32_t ms_block;

    /**
     * When not NULL, it should point to a buffer of at least 64KiB, where
     * libisofs will write the contents that should be written at the beginning
     * of a overwriteable media, to grow the image. The growing of an image is
     * a way, used by first time in growisofs by Andy Polyakov, to allow the
     * appending of new data to non-multisession media, such as DVD+RW, in the
     * same way you append a new session to a multisession disc, i.e., without
     * need to write again the contents of the previous image.
     *
     * Note that if you want this kind of image growing, you will also need to
     * set appendable to "1" and provide a valid ms_block after the previous
     * image.
     *
     * You should initialize the buffer either with 0s, or with the contents of
     * the first blocks of the image you're growing. In most cases, 0 is good
     * enought.
     */
    uint8_t *overwrite;

    /**
     * Size, in number of blocks, of the FIFO buffer used between the writer
     * thread and the burn_source. You have to provide at least a 32 blocks
     * buffer.
     */
    size_t fifo_size;
};

typedef struct ecma119_image Ecma119Image;
typedef struct ecma119_node Ecma119Node;
typedef struct joliet_node JolietNode;
typedef struct iso1999_node Iso1999Node;
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
    unsigned int iso1999 :1;

    /* allways write timestamps in GMT */
    unsigned int always_gmt :1;

    /* relaxed constraints */
    unsigned int omit_version_numbers :1;
    unsigned int allow_deep_paths :1;
    unsigned int allow_longer_paths :1;
    unsigned int max_37_char_filenames :1;
    unsigned int no_force_dots :1;
    unsigned int allow_lowercase :1;
    unsigned int allow_full_ascii :1;

    unsigned int relaxed_vol_atts : 1;

    /** Allow paths on Joliet tree to be larger than 240 bytes */
    unsigned int joliet_longer_paths :1;

    /*
     * Mode replace. If one of these flags is set, the correspodent values are
     * replaced with values below.
     */
    unsigned int replace_uid :1;
    unsigned int replace_gid :1;
    unsigned int replace_file_mode :1;
    unsigned int replace_dir_mode :1;
    unsigned int replace_timestamps :1;

    uid_t uid;
    gid_t gid;
    mode_t file_mode;
    mode_t dir_mode;
    time_t timestamp;

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
     * ISO 9660:1999 related information
     */
    Iso1999Node *iso1999_root;
    size_t iso1999_ndirs;
    uint32_t iso1999_path_table_size;
    uint32_t iso1999_l_path_table_pos;
    uint32_t iso1999_m_path_table_pos;

    /*
     * El-Torito related information
     */
    struct el_torito_boot_catalog *catalog;
    IsoFileSrc *cat; /**< location of the boot catalog in the new image */
    IsoFileSrc *bootimg; /**< location of the boot image in the new image */

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

/* ECMA-119, 8.2 */
struct ecma119_boot_rec_vol_desc
{
    uint8_t vol_desc_type            BP(1, 1);
    uint8_t std_identifier           BP(2, 6);
    uint8_t vol_desc_version         BP(7, 7);
    uint8_t boot_sys_id              BP(8, 39);
    uint8_t boot_id                  BP(40, 71);
    uint8_t boot_catalog             BP(72, 75);
    uint8_t unused                   BP(76, 2048);
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
