/*
 * Copyright (c) 2008 Vreixo Formoso
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "system_area.h"
#include "eltorito.h"
#include "filesrc.h"

#include <string.h>



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

int iso_write_system_area(Ecma119Image *t, uint8_t *buf)
{
    if ((t == NULL) || (buf == NULL)) {
        return ISO_NULL_POINTER;
    }

    /* set buf to 0s */
    memset(buf, 0, 16 * BLOCK_SIZE);

    if (t->catalog != NULL && t->catalog->image->isolinux_options & 0x02) {
        /* We need to write a MBR for an hybrid image */
        int ret;
        int img_blocks;

        img_blocks = t->curblock;
        ret = make_isohybrid_mbr(t->bootimg->sections[0].block, &img_blocks, (char*)buf, 0);

        // FIXME the new img_blocks size should be taken into account

        if (ret != 1) {
            /* error, it should never happen */
            return ISO_ASSERT_FAILURE;
        }
    }
    return ISO_SUCCESS;
}
