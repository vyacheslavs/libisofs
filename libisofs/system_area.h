/*
 * Copyright (c) 2008 Vreixo Formoso
 * Copyright (c) 2012 Thomas Schmitt
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 */

/*
 * Functions for dealing with the system area, this is, the first 16 blocks
 * of the image.
 *
 * At this time, this is only used for hybrid boot images with isolinux.
 */

#ifndef SYSTEM_AREA_H_
#define SYSTEM_AREA_H_

#include "ecma119.h"

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

/**
 * Write the system area for the given image to the given buffer.
 *
 * @param buf
 *      A buffer with at least 32 K allocated
 * @return
 *      1 if success, < 0 on error
 */
int iso_write_system_area(Ecma119Image *t, uint8_t *buf);

/**
 * Adjust t->tail_blocks to the eventual alignment needs of isohybrid booting.
 */
int iso_align_isohybrid(Ecma119Image *t, int flag);


/** 
 * Read the necessary ELF information from the first MIPS boot file.
 * See doc/boot_sectors.txt "DEC Boot Block" for "MIPS Little Endian".
 */
int iso_read_mipsel_elf(Ecma119Image *t, int flag);


/* Compute size and position of appended partitions.
*/
int iso_compute_append_partitions(Ecma119Image *t, int flag);


/* The parameter struct for production of a single Apple Partition Map entry.
   See also the partial APM description in doc/boot_sectors.txt.
   The list of entries is stored in Ecma119Image.apm_req.
   The size of a block can be chosen by setting Ecma119Image.apm_block_size.
   If an entry has start_block <=1, then its block_count will be adjusted
   to the final size of the partition map.
   If no such entry is requested, then it will be prepended automatically
   with name "Apple" and type "Apple_partition_map".
*/
struct iso_apm_partition_request {

    /* Always given in blocks of 2 KiB.
       Written to the ISO image according to Ecma119Image.apm_block_size.
    */
    uint32_t start_block;
    uint32_t block_count;

    /* All 32 bytes get copied to the system area.
       Take care to pad up short strings by 0.
    */
    uint8_t name[32];
    uint8_t type[32];
};

/* Copies the content of req and registers it in t.apm_req[].
   I.e. after the call the submitted storage of req can be disposed or re-used.
   Submit 0 as value flag.
*/
int iso_register_apm_entry(Ecma119Image *t,
                           struct iso_apm_partition_request *req, int flag);

/* Convenience frontend for iso_register_apm_entry().
   name and type are 0-terminated strings, which may get silently truncated.
*/
int iso_quick_apm_entry(Ecma119Image *t, 
           uint32_t start_block, uint32_t block_count, char *name, char *type);


/* CRC-32 as of GPT and Ethernet.
*/
unsigned int iso_crc32_gpt(unsigned char *data, int count, int flag);


/* These two pseudo-random generators produce byte strings which will
   surely not duplicate in the first 256 calls. If more calls are necessary
   in the same process, then one must wait until the output of
   gettimeofday(2) changes.
   It is advised to obtain them as late as possible, so that Ecma119Image *t
   can distinguish itself from other image production setups which might be
   run on other machines with the same process number at the same time.
*/

/* Produces a weakly random variation of a hardcoded real random uuid
*/
void iso_random_uuid(Ecma119Image *t, uint8_t uuid[16]);

/* Produces a weakly random variation of a hardcoded real random template
*/
void iso_random_8byte(Ecma119Image *t, uint8_t result[8]);


#endif /* SYSTEM_AREA_H_ */
