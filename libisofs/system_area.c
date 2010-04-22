/*
 * Copyright (c) 2008 Vreixo Formoso
 * Copyright (c) 2010 Thomas Schmitt
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 */

#include "system_area.h"
#include "eltorito.h"
#include "filesrc.h"

#include <string.h>
#include <stdio.h>


/*
 * Create a MBR for an isohybrid enabled ISOLINUX boot image.
 * See libisofs/make_isohybrid_mbr.c
 * Deprecated.
 */
int make_isohybrid_mbr(int bin_lba, int *img_blocks, char *mbr, int flag);

/*
 * The New ISOLINUX MBR Producer.
 * Be cautious with changing parameters. Only few combinations are tested.
 *
 */
int make_isolinux_mbr(int32_t *img_blocks, uint32_t boot_lba,
                      uint32_t mbr_id, int head_count, int sector_count,
                      int part_offset, int part_number, int fs_type,
                      uint8_t *buf, int flag);


/* This is the gesture of grub-mkisofs --protective-msdos-label as explained by
   Vladimir Serbinenko <phcoder@gmail.com>, 2 April 2010, on grub-devel@gnu.org
   "Currently we use first and not last entry. You need to:
    1) Zero-fill 446-510
    2) Put 0x55, 0xAA into 510-512
    3) Put 0x80 (for bootable partition), 0, 2, 0 (C/H/S of the start), 0xcd
      (partition type), [3 bytes of C/H/S end], 0x01, 0x00, 0x00, 0x00 (LBA
      start in little endian), [LBA end in little endian] at 446-462
   "

   "C/H/S end" means the CHS address of the last block in the partition.
   It seems that not "[LBA end in little endian]" but "number of blocks"
   should go into bytes 458-461. But with a start lba of 1, this is the
   same number.
   See also http://en.wikipedia.org/wiki/Master_boot_record
*/
static
int make_grub_msdos_label(int img_blocks, uint8_t *buf, int flag)
{
    uint8_t *wpt;
    unsigned long end_lba, secs, end_sec, end_head, end_cyl;
    int sph = 63, hpc = 255, i;

    /* Partition table unit is 512 bytes per sector, ECMA-119 unit is 2048 */
    if (img_blocks >= 0x40000000)
      img_blocks = 0x40000000 - 1;         /* truncate rather than roll over */
    secs = end_lba = img_blocks * 4 - 1;   /* last valid 512-lba */
    end_cyl = secs / (sph * hpc);
    secs -= end_cyl * sph * hpc;
    end_head = secs / sph;
    end_sec = secs - end_head * sph + 1;   /* Sector count starts by 1 */
    if (end_cyl >= 1024) {
        end_cyl = 1023;
        end_head = hpc - 1;
        end_sec = sph;
    }

    /* 1) Zero-fill 446-510 */
    wpt = buf + 446;
    memset(wpt, 0, 64);

    /* 2) Put 0x55, 0xAA into 510-512 (actually 510-511) */
    buf[510] = 0x55;
    buf[511] = 0xAA;

    /* 3) Put 0x80 (for bootable partition), */
    *(wpt++) = 0x80;

    /* 0, 2, 0 (C/H/S of the start), */
    *(wpt++) = 0;
    *(wpt++) = 2;
    *(wpt++) = 0;

    /* 0xcd (partition type) */
    *(wpt++) = 0xcd;

    /* [3 bytes of C/H/S end], */
    *(wpt++) = end_head;
    *(wpt++) = end_sec | ((end_cyl & 0x300) >> 2);
    *(wpt++) = end_cyl & 0xff;
    

    /* 0x01, 0x00, 0x00, 0x00 (LBA start in little endian), */
    *(wpt++) = 0x01;
    *(wpt++) = 0x00;
    *(wpt++) = 0x00;
    *(wpt++) = 0x00;

    /* [LBA end in little endian] */
    for (i = 0; i < 4; i++)
       *(wpt++) = (end_lba >> (8 * i)) & 0xff;

    /* at 446-462 */
    if (wpt - buf != 462) {
        fprintf(stderr,
        "libisofs: program error in make_grub_msdos_label: \"assert 462\"\n");
        return ISO_ASSERT_FAILURE;
    }
    return ISO_SUCCESS;
}


int iso_write_system_area(Ecma119Image *t, uint8_t *buf)
{
    int ret;
    int img_blocks;

    if ((t == NULL) || (buf == NULL)) {
        return ISO_NULL_POINTER;
    }

    /* set buf to 0s */
    memset(buf, 0, 16 * BLOCK_SIZE);

    img_blocks = t->curblock;
    if (t->system_area_data != NULL) {
        /* Write more or less opaque boot image */
        memcpy(buf, t->system_area_data, 16 * BLOCK_SIZE);

    } else if (t->catalog != NULL &&
               (t->catalog->bootimages[0]->isolinux_options & 0x0a) == 0x02) {
        /* Check for isolinux image with magic number of 3.72 and produce
           an MBR from our built-in template. (Deprecated since 31 Mar 2010)
        */
        ret = make_isohybrid_mbr(t->bootsrc[0]->sections[0].block,
                                 &img_blocks, (char*)buf, 0);
        if (ret != 1) {
            /* error, it should never happen */
            return ISO_ASSERT_FAILURE;
        }
        return ISO_SUCCESS;
    }
    if (t->system_area_options & 1) {
        /* Write GRUB protective msdos label, i.e. a isimple partition table */
        ret = make_grub_msdos_label(img_blocks, buf, 0);
        if (ret != 1) /* error should never happen */
            return ISO_ASSERT_FAILURE;
    } else if(t->system_area_options & 2) {
        /* Patch externally provided system area as isohybrid MBR */
        if (t->catalog == NULL || t->system_area_data == NULL) {
            /* isohybrid makes only sense together with ISOLINUX boot image
               and externally provided System Area.
            */
            return ISO_ISOLINUX_CANT_PATCH;
        }
        ret = make_isolinux_mbr(&img_blocks, t->bootsrc[0]->sections[0].block,
                                (uint32_t) 0, 64, 32, 0, 1, 0x17, buf, 1);
        if (ret != 1)
            return ret;
    }
    return ISO_SUCCESS;
}
