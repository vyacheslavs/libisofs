/*
 * Copyright (c) 2008 Vreixo Formoso
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

#endif /* SYSTEM_AREA_H_ */
