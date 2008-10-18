/*
 * Copyright (c) 2007 Vreixo Formoso
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See COPYING file for details.
 */

/**
 * Declare El-Torito related structures.
 * References:
 *  "El Torito" Bootable CD-ROM Format Specification Version 1.0 (1995)
 */

#ifndef LIBISO_ELTORITO_H
#define LIBISO_ELTORITO_H

#include "ecma119.h"
#include "node.h"

/**
 * A node that acts as a placeholder for an El-Torito catalog.
 */
struct Iso_Boot
{
    IsoNode node;
};

struct el_torito_boot_catalog {
    IsoBoot *node; /* node of the catalog */
    struct el_torito_boot_image *image; /* default boot image */
};

struct el_torito_boot_image {
    IsoFile *image;

    unsigned int bootable:1; /**< If the entry is bootable. */
    unsigned int isolinux:1; /**< If the image will be patched */
    unsigned char type; /**< The type of image */
    unsigned char partition_type; /**< type of partition for HD-emul images */
    short load_seg; /**< Load segment for the initial boot image. */
    short load_size; /**< Number of sectors to load. */
};

/** El-Torito, 2.1 */
struct el_torito_validation_entry {
    uint8_t header_id           BP(1, 1);
    uint8_t platform_id         BP(2, 2);
    uint8_t reserved            BP(3, 4);
    uint8_t id_string           BP(5, 28);
    uint8_t checksum            BP(29, 30);
    uint8_t key_byte1           BP(31, 31);
    uint8_t key_byte2           BP(32, 32);
};

/** El-Torito, 2.2 */
struct el_torito_default_entry {
    uint8_t boot_indicator      BP(1, 1);
    uint8_t boot_media_type     BP(2, 2);
    uint8_t load_seg            BP(3, 4);
    uint8_t system_type         BP(5, 5);
    uint8_t unused1             BP(6, 6);
    uint8_t sec_count           BP(7, 8);
    uint8_t block               BP(9, 12);
    uint8_t unused2             BP(13, 32);
};

/** El-Torito, 2.3 */
struct el_torito_section_header {
    uint8_t header_indicator    BP(1, 1);
    uint8_t platform_id         BP(2, 2);
    uint8_t number              BP(3, 4);
    uint8_t character           BP(5, 32);
};

/** El-Torito, 2.4 */
struct el_torito_section_entry {
    uint8_t boot_indicator      BP(1, 1);
    uint8_t boot_media_type     BP(2, 2);
    uint8_t load_seg            BP(3, 4);
    uint8_t system_type         BP(5, 5);
    uint8_t unused1             BP(6, 6);
    uint8_t sec_count           BP(7, 8);
    uint8_t block               BP(9, 12);
    uint8_t selec_criteria      BP(13, 13);
    uint8_t vendor_sc           BP(14, 32);
};

void el_torito_boot_catalog_free(struct el_torito_boot_catalog *cat);

/**
 * Create a IsoFileSrc for writing the el-torito catalog for the given
 * target, and add it to target. If the target already has a src for the
 * catalog, it just returns.
 */
int el_torito_catalog_file_src_create(Ecma119Image *target, IsoFileSrc **src);

/**
 * Create a writer for el-torito information.
 */
int eltorito_writer_create(Ecma119Image *target);

/*
 * Create a MBR for an isohybrid enabled ISOLINUX boot image.
 *
 * It is assumed that the caller has verified the readiness of the boot image
 * by checking for 0xfb 0xc0 0x78 0x70 at bytes 0x40 to 0x43 of isolinux.bin.
 *
 * @param bin_lba    The predicted LBA of isolinux.bin within the emerging ISO.
 * @param img_blocks The predicted number of 2048 byte blocks in the ISO.
 *                   It will get rounded up to full MBs and that many blocks
 *                   must really be written as ISO 9660 image.
 * @param mbr        A buffer of at least 512 bytes to take the result which is
 *                   to be written as the very beginning of the ISO.
 * @param flag    unused yet, submit 0
 * @return  <0 = fatal, 0 = failed , 1 = ok , 2 = ok with size warning
 */
int make_isohybrid_mbr(int bin_lba, int *img_blocks, char *mbr, int flag);

#endif /* LIBISO_ELTORITO_H */
