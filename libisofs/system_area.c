/*
 * Copyright (c) 2008 Vreixo Formoso
 * Copyright (c) 2010 Thomas Schmitt
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "libisofs.h"
#include "system_area.h"
#include "eltorito.h"
#include "filesrc.h"
#include "ecma119_tree.h"
#include "image.h"
#include "messages.h"
#include "ecma119.h"

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


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
int make_isolinux_mbr(uint32_t *img_blocks, uint32_t boot_lba,
                      uint32_t mbr_id, int head_count, int sector_count,
                      int part_offset, int part_number, int fs_type,
                      uint8_t *buf, int flag);


/*
 * @param flag bit0= img_blocks is start address rather than end address:
                     do not subtract 1
 */
static
void iso_compute_cyl_head_sec(uint32_t *img_blocks, int hpc, int sph,
                              uint32_t *end_lba, uint32_t *end_sec,
                              uint32_t *end_head, uint32_t *end_cyl, int flag)
{
    uint32_t secs;

    /* Partition table unit is 512 bytes per sector, ECMA-119 unit is 2048 */
    if (*img_blocks >= 0x40000000)
      *img_blocks = 0x40000000 - 1;        /* truncate rather than roll over */
    if (flag & 1)
        secs = *end_lba = *img_blocks * 4;            /* first valid 512-lba */
    else
        secs = *end_lba = *img_blocks * 4 - 1;         /* last valid 512-lba */
    *end_cyl = secs / (sph * hpc);
    secs -= *end_cyl * sph * hpc;
    *end_head = secs / sph;
    *end_sec = secs - *end_head * sph + 1;   /* Sector count starts by 1 */
    if (*end_cyl >= 1024) {
        *end_cyl = 1023;
        *end_head = hpc - 1;
        *end_sec = sph;
    }
}


/* Compute size and position of appended partitions.
*/
int iso_compute_append_partitions(Ecma119Image *t, int flag)
{
    int ret, i, sa_type;
    uint32_t pos, size, add_pos = 0;
    struct stat stbuf;

    sa_type = (t->system_area_options >> 2) & 0x3f;
    pos = (t->vol_space_size + t->ms_block);
    for (i = 0; i < ISO_MAX_PARTITIONS; i++) {
        if (t->appended_partitions[i] == NULL)
    continue;
        if (t->appended_partitions[i][0] == 0)
    continue;
        ret = stat(t->appended_partitions[i], &stbuf);
        if (ret == -1)
            return ISO_BAD_PARTITION_FILE;
        if (! S_ISREG(stbuf.st_mode))
            return ISO_BAD_PARTITION_FILE;
        size = ((stbuf.st_size + 2047) / 2048);
        if (sa_type == 3 && (pos % ISO_SUN_CYL_SIZE))
            add_pos = ISO_SUN_CYL_SIZE - (pos % ISO_SUN_CYL_SIZE);
        t->appended_part_prepad[i] = add_pos;
        t->appended_part_start[i] = pos + add_pos;
        t->appended_part_size[i] = size;
        pos += add_pos + size;
        t->total_size += (add_pos + size) * 2048;
    }
    return ISO_SUCCESS;
}


/* Note: partition_offset and partition_size are counted in 2048 blocks
 */
static int write_mbr_partition_entry(int partition_number, int partition_type,
                  uint32_t partition_offset, uint32_t partition_size,
                  int sph, int hpc, uint8_t *buf, int flag)
{
    uint8_t *wpt;
    uint32_t end_lba, end_sec, end_head, end_cyl;
    uint32_t start_lba, start_sec, start_head, start_cyl;
    uint32_t after_end;
    int i;

    after_end = partition_offset + partition_size;
    iso_compute_cyl_head_sec(&partition_offset, hpc, sph,
                           &start_lba, &start_sec, &start_head, &start_cyl, 1);
    iso_compute_cyl_head_sec(&after_end, hpc, sph,
                             &end_lba, &end_sec, &end_head, &end_cyl, 0);
    wpt = buf + 446 + (partition_number - 1) * 16;

    /* Not bootable */
    *(wpt++) = 0x00;

    /* C/H/S of the start */
    *(wpt++) = start_head;
    *(wpt++) = start_sec | ((start_cyl & 0x300) >> 2);
    *(wpt++) = start_cyl & 0xff;

    /* (partition type) */
    *(wpt++) = partition_type;

    /* 3 bytes of C/H/S end */
    *(wpt++) = end_head;
    *(wpt++) = end_sec | ((end_cyl & 0x300) >> 2);
    *(wpt++) = end_cyl & 0xff;
    
    /* LBA start in little endian */
    for (i = 0; i < 4; i++)
       *(wpt++) = (start_lba >> (8 * i)) & 0xff;

    /* Number of sectors in partition, little endian */
    end_lba = end_lba - start_lba + 1;
    for (i = 0; i < 4; i++)
       *(wpt++) = (end_lba >> (8 * i)) & 0xff;

    /* Afaik, partition tables are recognize donly with MBR signature */
    buf[510] = 0x55;
    buf[511] = 0xAA;

    return ISO_SUCCESS;
}


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

   flag   bit0= do not write 0x55, 0xAA to 510,511
          bit1= do not mark partition as bootable
*/
static
int make_grub_msdos_label(uint32_t img_blocks, int sph, int hpc,
                          uint8_t *buf, int flag)
{
    uint8_t *wpt;
    uint32_t end_lba, end_sec, end_head, end_cyl;
    int i;

    iso_compute_cyl_head_sec(&img_blocks, hpc, sph,
                             &end_lba, &end_sec, &end_head, &end_cyl, 0);

    /* 1) Zero-fill 446-510 */
    wpt = buf + 446;
    memset(wpt, 0, 64);

    if (!(flag & 1)) {
        /* 2) Put 0x55, 0xAA into 510-512 (actually 510-511) */
        buf[510] = 0x55;
        buf[511] = 0xAA;
    }
    if (!(flag & 2)) {
      /* 3) Put 0x80 (for bootable partition), */
      *(wpt++) = 0x80;
    } else {
      *(wpt++) = 0;
    }

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


/* @param flag bit0= zeroize partitions entries 2, 3, 4
*/
static
int iso_offset_partition_start(uint32_t img_blocks, uint32_t partition_offset,
                               int sph, int hpc, uint8_t *buf, int flag)
{
    uint8_t *wpt;
    uint32_t end_lba, end_sec, end_head, end_cyl;
    uint32_t start_lba, start_sec, start_head, start_cyl;
    int i;

    iso_compute_cyl_head_sec(&partition_offset, hpc, sph,
                           &start_lba, &start_sec, &start_head, &start_cyl, 1);
    iso_compute_cyl_head_sec(&img_blocks, hpc, sph,
                             &end_lba, &end_sec, &end_head, &end_cyl, 0);
    wpt = buf + 446;

    /* Let pass only legal bootability values */
    if (*wpt != 0 && *wpt != 0x80)
        (*wpt) = 0;
    wpt++;

    /* C/H/S of the start */
    *(wpt++) = start_head;
    *(wpt++) = start_sec | ((start_cyl & 0x300) >> 2);
    *(wpt++) = start_cyl & 0xff;

    /* (partition type) */
    wpt++;

    /* 3 bytes of C/H/S end */
    *(wpt++) = end_head;
    *(wpt++) = end_sec | ((end_cyl & 0x300) >> 2);
    *(wpt++) = end_cyl & 0xff;
    
    /* LBA start in little endian */
    for (i = 0; i < 4; i++)
       *(wpt++) = (start_lba >> (8 * i)) & 0xff;

    /* Number of sectors in partition, little endian */
    end_lba = end_lba - start_lba + 1;
    for (i = 0; i < 4; i++)
       *(wpt++) = (end_lba >> (8 * i)) & 0xff;

    if (wpt - buf != 462) {
        fprintf(stderr,
    "libisofs: program error in iso_offset_partition_start: \"assert 462\"\n");
        return ISO_ASSERT_FAILURE;
    }

    if (flag & 1) /* zeroize the other partition entries */
        memset(wpt, 0, 3 * 16);

    return ISO_SUCCESS;
}


static int boot_nodes_from_iso_path(Ecma119Image *t, char *path, 
                                   IsoNode **iso_node, Ecma119Node **ecma_node,
                                   char *purpose, int flag)
{
    int ret;

    ret = iso_tree_path_to_node(t->image, path, iso_node);
    if (ret < 0) {
        iso_msg_submit(t->image->id, ISO_BOOT_FILE_MISSING, 0,
                       "Cannot find %s '%s'", purpose, path);
        return ISO_BOOT_FILE_MISSING;
    }
    if ((*iso_node)->type != LIBISO_FILE) {
        iso_msg_submit(t->image->id, ISO_BOOT_IMAGE_NOT_VALID, 0,
                       "Designated boot file is not a data file: '%s'", path);
        return ISO_BOOT_IMAGE_NOT_VALID;
    }

    *ecma_node= ecma119_search_iso_node(t, *iso_node);
    if (*ecma_node == NULL) {
        iso_msg_submit(t->image->id, ISO_BOOT_IMAGE_NOT_VALID, 0,
                      "Program error: IsoFile has no Ecma119Node: '%s'", path);
        return ISO_ASSERT_FAILURE;
    } else {
        if ((*ecma_node)->type != ECMA119_FILE) {
            iso_msg_submit(t->image->id, ISO_BOOT_IMAGE_NOT_VALID, 0,
              "Program error: Ecma119Node of IsoFile is no ECMA119_FILE: '%s'",
                          path);
            return ISO_ASSERT_FAILURE;
        }
    }
    return ISO_SUCCESS;
}


/* This function was implemented according to doc/boot_sectors.txt section
   "MIPS Volume Header" which was derived by Thomas Schmitt from
   cdrkit-1.1.10/genisoimage/boot-mips.c by Steve McIntyre which is based
   on work of Florian Lohoff and Thiemo Seufer who possibly learned from
   documents of MIPS Computer Systems, Inc. and Silicon Graphics Computer
   Systems, Inc.
   This function itself is entirely under copyright (C) 2010 Thomas Schmitt.
*/
static int make_mips_volume_header(Ecma119Image *t, uint8_t *buf, int flag)
{
    char *namept, *name_field;
    uint32_t num_cyl, idx, blocks, num, checksum;
    off_t image_size;
    static uint32_t bps = 512, spt = 32;
    Ecma119Node *ecma_node;
    IsoNode *node;
    IsoStream *stream;
    off_t file_size;
    uint32_t file_lba;
    int ret;

    /* Bytes 512 to 32767 may come from image or external file */
    memset(buf, 0, 512);

    image_size = t->curblock * 2048;

    /* 0 -   3 | 0x0be5a941 | Magic number */
    iso_msb(buf, 0x0be5a941, 4);

    /* 28 -  29 |  num_cyl_l | Number of usable cylinder, lower two bytes */
    num_cyl = (image_size + (bps * spt) - 1) / (bps * spt);
    iso_msb(buf + 28, num_cyl & 0xffff, 2);

    /* 32 -  33 |          1 | Number of tracks per cylinder */
    iso_msb(buf + 32, 1, 2);

    /* 35 -  35 |  num_cyl_h | Number of usable cylinders, high byte */
    buf[35] = (num_cyl >> 16) & 0xff;
    
    /* 38 -  39 |         32 | Sectors per track */
    iso_msb(buf + 38, spt, 2);

    /* 40 -  41 |        512 | Bytes per sector */
    iso_msb(buf + 40, bps, 2);

    /* 44 -  47 | 0x00000034 | Controller characteristics */
    iso_msb(buf + 44, 0x00000034, 4);

    /*  72 -  87 | ========== | Volume Directory Entry 1 */
    /*  72 -  79 |  boot_name | Boot file basename */
    /*  80 -  83 | boot_block | ISO 9660 LBA of boot file * 4 */
    /*  84 -  87 | boot_bytes | File length in bytes */
    /*  88 - 311 |          0 | Volume Directory Entries 2 to 15 */

    for (idx = 0; idx < t->image->num_mips_boot_files; idx++) {
        ret = boot_nodes_from_iso_path(t, t->image->mips_boot_file_paths[idx], 
                                       &node, &ecma_node, "MIPS boot file", 0);
        if (ret < 0)
            return ret;

        namept = (char *) iso_node_get_name(node);
        name_field = (char *) (buf + (72 + 16 * idx));
        strncpy(name_field, namept, 8);

        file_lba = ecma_node->info.file->sections[0].block;

        iso_msb(buf + (72 + 16 * idx) + 8, file_lba * 4, 4);

        stream = iso_file_get_stream((IsoFile *) node);
        file_size = iso_stream_get_size(stream);

        /* Shall i really round up to 2048 ? Steve says yes.*/
        iso_msb(buf + (72 + 16 * idx) + 12,
                ((file_size + 2047) / 2048 ) * 2048, 4);
        
    }

    /* 408 - 411 |  part_blks | Number of 512 byte blocks in partition */
    blocks = (image_size + bps - 1) / bps;
    iso_msb(buf + 408, blocks, 4);
    /* 416 - 419 |          0 | Partition is volume header */
    iso_msb(buf + 416, 0, 4);

    /* 432 - 435 |  part_blks | Number of 512 byte blocks in partition */
    iso_msb(buf + 432, blocks, 4);
    iso_msb(buf + 444, 6, 4);

    /* 504 - 507 |   head_chk | Volume header checksum  
                                The two's complement of bytes 0 to 503 read
                                as big endian unsigned 32 bit:
                                  sum(32-bit-words) + head_chk == 0
    */
    checksum = 0;
    for (idx = 0; idx < 504; idx += 4) {
        num = iso_read_msb(buf + idx, 4);
        /* Addition modulo a natural number is commutative and associative.
           Thus the inverse of a sum is the sum of the inverses of the addends.
        */
        checksum -= num;
    }
    iso_msb(buf + 504, checksum, 4);

    return ISO_SUCCESS;
}


/* The following two functions were implemented according to
   doc/boot_sectors.txt section "MIPS Little Endian" which was derived
   by Thomas Schmitt from
   cdrkit-1.1.10/genisoimage/boot-mipsel.c by Steve McIntyre which is based
   on work of Florian Lohoff and Thiemo Seufer,
   and from <elf.h> by Free Software Foundation, Inc.

   Both functions are entirely under copyright (C) 2010 Thomas Schmitt.
*/

/**
 * Read the necessary ELF information from the first MIPS boot file.
 * This is done before image writing starts.
 */
int iso_read_mipsel_elf(Ecma119Image *t, int flag)
{
    uint32_t phdr_adr, todo, count;
    int ret;
    uint8_t elf_buf[2048];
    IsoNode *iso_node;
    Ecma119Node *ecma_node;
    IsoStream *stream;
    
    if (t->image->num_mips_boot_files <= 0)
        return ISO_SUCCESS;

    ret = boot_nodes_from_iso_path(t, t->image->mips_boot_file_paths[0],
                                   &iso_node, &ecma_node, "MIPS boot file", 0);
    if (ret < 0)
        return ret;
    stream = iso_file_get_stream((IsoFile *) iso_node);

    ret = iso_stream_open(stream);
    if (ret < 0) {
        iso_msg_submit(t->image->id, ret, 0,
                       "Cannot open designated MIPS boot file '%s'",
                       t->image->mips_boot_file_paths[0]);
        return ret;
    }
    ret = iso_stream_read(stream, elf_buf, 32);
    if (ret != 32) {
cannot_read:;
        iso_stream_close(stream);
        iso_msg_submit(t->image->id, ret, 0,
                       "Cannot read from designated MIPS boot file '%s'",
                       t->image->mips_boot_file_paths[0]);
        return ret;
    }


    /*  24 -  27 |    e_entry | Entry point virtual address */
    t->mipsel_e_entry = iso_read_lsb(elf_buf + 24, 4);

    /* 28 -  31 |    e_phoff | Program header table file offset */
    phdr_adr = iso_read_lsb(elf_buf + 28, 4);

    /* Skip stream up to byte address phdr_adr */
    todo = phdr_adr - 32;
    while (todo > 0) {
        if (todo > 2048)
            count = 2048;
        else
            count = todo;
        todo -= count;
        ret = iso_stream_read(stream, elf_buf, count);
        if (ret != count)
            goto cannot_read;
    }
    ret = iso_stream_read(stream, elf_buf, 20);
    if (ret != 20)
        goto cannot_read;

    /*  4 -   7 |   p_offset | Segment file offset */
    t->mipsel_p_offset = iso_read_lsb(elf_buf + 4, 4);

    /*  8 -  11 |    p_vaddr | Segment virtual address */
    t->mipsel_p_vaddr = iso_read_lsb(elf_buf + 8, 4);

    /* 16 -  19 |   p_filesz | Segment size in file */
    t->mipsel_p_filesz = iso_read_lsb(elf_buf + 16, 4);

    iso_stream_close(stream);
    return ISO_SUCCESS;
}


/**
 * Write DEC Bootblock from previously read ELF parameters.
 * This is done when image writing has already begun.
 */
static int make_mipsel_boot_block(Ecma119Image *t, uint8_t *buf, int flag)
{
    int ret;
    uint32_t seg_size, seg_start;
    IsoNode *iso_node;
    Ecma119Node *ecma_node;
    
    /* Bytes 512 to 32767 may come from image or external file */
    memset(buf, 0, 512);

    if (t->image->num_mips_boot_files <= 0)
        return ISO_SUCCESS;

    ret = boot_nodes_from_iso_path(t, t->image->mips_boot_file_paths[0],
                                   &iso_node, &ecma_node, "MIPS boot file", 0);
    if (ret < 0)
        return ret;

    /*  8 -  11 | 0x0002757a | Magic number */
    iso_lsb(buf + 8, 0x0002757a, 4);

    /* 12 -  15 |          1 | Mode  1: Multi extent boot */
    iso_lsb(buf + 12, 1, 4);

    /* 16 -  19 |   load_adr | Load address */
    iso_lsb(buf + 16, t->mipsel_p_vaddr, 4);

    /* 20 -  23 |   exec_adr | Execution address */
    iso_lsb(buf + 20, t->mipsel_e_entry, 4);

    /* 24 -  27 |   seg_size | Segment size in file. */
    seg_size = (t->mipsel_p_filesz + 511) / 512;
    iso_lsb(buf + 24, seg_size, 4);
    
    /* 28 -  31 |  seg_start | Segment file offset */
    seg_start = ecma_node->info.file->sections[0].block * 4
                + (t->mipsel_p_offset + 511) / 512;
    iso_lsb(buf + 28, seg_start, 4);
    
    return ISO_SUCCESS;
}


/* The following two functions were implemented according to
   doc/boot_sectors.txt section "SUN Disk Label and boot images" which
   was derived by Thomas Schmitt from
     cdrtools-2.01.01a77/mkisofs/sunlabel.h
     cdrtools-2.01.01a77/mkisofs/mkisofs.8
     by Joerg Schilling

   Both functions are entirely under copyright (C) 2010 Thomas Schmitt.
*/

/* @parm flag bit0= copy from next lower valid partition table entry
 */
static int write_sun_partition_entry(int partition_number,
                  char *appended_partitions[],
                  uint32_t partition_offset[], uint32_t partition_size[],
                  uint32_t cyl_size, uint8_t *buf, int flag)
{
    uint8_t *wpt;
    int read_idx, i;

    if (partition_number < 1 || partition_number > 8)
        return ISO_ASSERT_FAILURE;
    
    /* 142 - 173 | ========== | 8 partition entries of 4 bytes */
    wpt = buf + 142 + (partition_number - 1) * 4;
    if (partition_number == 1)
        iso_msb(wpt, 4, 2);                            /* 4 = User partition */
    else
        iso_msb(wpt, 2, 2);            /* 2 = Root partition with boot image */
    iso_msb(wpt + 2, 0x10, 2);              /* Permissions: 0x10 = read-only */

    /* 444 - 507 | ========== | Partition table */
    wpt = buf + 444 + (partition_number - 1) * 8;
    read_idx = partition_number - 1;
    if (flag & 1) {
        /* Search next lower valid partition table entry. #1 is default */
        for (read_idx = partition_number - 2; read_idx > 0; read_idx--)
            if (appended_partitions[read_idx] != NULL)
                if (appended_partitions[read_idx][0] != 0)
        break;
    }
    iso_msb(wpt, partition_offset[read_idx] / (uint32_t) ISO_SUN_CYL_SIZE, 4);
    iso_msb(wpt + 4, partition_size[read_idx] * 4, 4);

    /* 510 - 511 |   checksum | The result of exoring 2-byte words 0 to 254 */
    buf[510] = buf[511] = 0;
    for (i = 0; i < 510; i += 2) {
        buf[510] ^= buf[i];
        buf[511] ^= buf[i + 1];
    }

    return ISO_SUCCESS;
}

/**
 * Write SUN Disk Label with ISO in partition 1 and unused 2 to 8
 */
static int make_sun_disk_label(Ecma119Image *t, uint8_t *buf, int flag)
{
    int ret;

    /* Bytes 512 to 32767 may come from image or external file */
    memset(buf, 0, 512);

    /* 0 - 127 |      label | ASCII Label */
    if (t->ascii_disc_label[0])
        strncpy((char *) buf, t->ascii_disc_label, 128);
    else
        strcpy((char *) buf,
               "CD-ROM Disc with Sun sparc boot created by libisofs");
    
    /* 128 - 131 |          1 | Layout version */
    iso_msb(buf + 128, 1, 4);

    /* 140 - 141 |          8 | Number of partitions */
    iso_msb(buf + 140, 8, 2);

    /* 188 - 191 | 0x600ddeee | vtoc sanity */
    iso_msb(buf + 188, 0x600ddeee, 4);

    /* 420 - 421 |        350 | Rotations per minute */
    iso_msb(buf + 420, 350, 2);

    /* 422 - 423 |       2048 | Number of physical cylinders (fixely 640 MB) */
    iso_msb(buf + 422, 2048, 2);

    /* 430 - 431 |          1 | interleave factor */
    iso_msb(buf + 430, 1, 2);

    /* 432 - 433 |       2048 | Number of data cylinders (fixely 640 MB) */
    iso_msb(buf + 432, 2048, 2);

    /* 436 - 437 |          1 | Number of heads per cylinder (1 cyl = 320 kB)*/
    iso_msb(buf + 436, 1, 2);

    /* 438 - 439 |        640 | Number of sectors per head (1 head = 320 kB) */
    iso_msb(buf + 438, 640, 2);

    /* 508 - 509 |     0xdabe | Magic Number */
    iso_msb(buf + 508, 0xdabe, 2);

    /* Set partition 1 to describe ISO image and compute checksum */
    t->appended_part_start[0] = 0;
    t->appended_part_size[0] = t->curblock;
    ret = write_sun_partition_entry(1, t->appended_partitions,
                  t->appended_part_start, t->appended_part_size,
                  ISO_SUN_CYL_SIZE, buf, 0);
 
    return ISO_SUCCESS;
}


int iso_write_system_area(Ecma119Image *t, uint8_t *buf)
{
    int ret, int_img_blocks, sa_type, i, will_append = 0;
    int first_partition = 1, last_partition = 4;
    uint32_t img_blocks;

    if ((t == NULL) || (buf == NULL)) {
        return ISO_NULL_POINTER;
    }

    /* set buf to 0s */
    memset(buf, 0, 16 * BLOCK_SIZE);

    sa_type = (t->system_area_options >> 2) & 0x3f;
    if (sa_type == 3) {
        first_partition = 2;
        last_partition = 8;
    }
    for (i = first_partition - 1; i <= last_partition - 1; i++)
        if (t->appended_partitions[i] != NULL) {
            will_append = 1;
    break;
        }

    img_blocks = t->curblock;
    if (t->system_area_data != NULL) {
        /* Write more or less opaque boot image */
        memcpy(buf, t->system_area_data, 16 * BLOCK_SIZE);

    } else if (sa_type == 0 && t->catalog != NULL &&
               (t->catalog->bootimages[0]->isolinux_options & 0x0a) == 0x02) {
        /* Check for isolinux image with magic number of 3.72 and produce
           an MBR from our built-in template. (Deprecated since 31 Mar 2010)
        */
        if (img_blocks < 0x80000000) {
            int_img_blocks= img_blocks;
        } else {
            int_img_blocks= 0x7ffffff0;
        }
        ret = make_isohybrid_mbr(t->bootsrc[0]->sections[0].block,
                                 &int_img_blocks, (char*)buf, 0);
        if (ret != 1) {
            /* error, it should never happen */
            return ISO_ASSERT_FAILURE;
        }
        return ISO_SUCCESS;
    }
    if (sa_type == 0 && (t->system_area_options & 1)) {
        /* Write GRUB protective msdos label, i.e. a simple partition table */
        ret = make_grub_msdos_label(img_blocks, t->partition_secs_per_head,
                                    t->partition_heads_per_cyl, buf, 0);
        if (ret != ISO_SUCCESS) /* error should never happen */
            return ISO_ASSERT_FAILURE;
    } else if(sa_type == 0 && (t->system_area_options & 2)) {
        /* Patch externally provided system area as isohybrid MBR */
        if (t->catalog == NULL || t->system_area_data == NULL) {
            /* isohybrid makes only sense together with ISOLINUX boot image
               and externally provided System Area.
            */
            return ISO_ISOLINUX_CANT_PATCH;
        }
        ret = make_isolinux_mbr(&img_blocks, t->bootsrc[0]->sections[0].block,
                               (uint32_t) 0, t->partition_heads_per_cyl,
                               t->partition_secs_per_head, 0, 1, 0x17, buf, 1);
        if (ret != 1)
            return ret;
    } else if (sa_type == 1) {
        ret = make_mips_volume_header(t, buf, 0);
        if (ret != ISO_SUCCESS)
            return ret;
    } else if (sa_type == 2) {
        ret = make_mipsel_boot_block(t, buf, 0);
        if (ret != ISO_SUCCESS)
            return ret;
    } else if ((t->partition_offset > 0 || will_append) && sa_type == 0) {
        /* Write a simple partition table. */
        ret = make_grub_msdos_label(img_blocks, t->partition_secs_per_head,
                                    t->partition_heads_per_cyl, buf, 2);
        if (ret != ISO_SUCCESS) /* error should never happen */
            return ISO_ASSERT_FAILURE;
        if (t->partition_offset == 0) {
            /* Re-write partion entry 1 : start at 0, type Linux */
            ret = write_mbr_partition_entry(1, 0x83, 0, img_blocks,
                        t->partition_secs_per_head, t->partition_heads_per_cyl,
                        buf, 0);
            if (ret < 0)
                return ret;
        }
    } else if (sa_type == 3) {
        ret = make_sun_disk_label(t, buf, 0);
        if (ret != ISO_SUCCESS)
            return ret;
    }

    if (t->partition_offset > 0 && sa_type == 0) {
        /* Adjust partition table to partition offset */
        img_blocks = t->curblock;                  /* value might be altered */
        ret = iso_offset_partition_start(img_blocks, t->partition_offset,
                                         t->partition_secs_per_head,
                                         t->partition_heads_per_cyl, buf, 1);
        if (ret != ISO_SUCCESS) /* error should never happen */
            return ISO_ASSERT_FAILURE;
    }

    /* This eventually overwrites the partition table entries made so far */
    for (i = first_partition - 1; i <= last_partition - 1; i++) {
        if (t->appended_partitions[i] == NULL)
    continue;
        if (sa_type == 3) {
          ret = write_sun_partition_entry(i + 1, t->appended_partitions,
                        t->appended_part_start, t->appended_part_size,
                        ISO_SUN_CYL_SIZE,
                        buf, t->appended_partitions[i][0] == 0);
        } else {
          ret = write_mbr_partition_entry(i + 1, t->appended_part_types[i],
                        t->appended_part_start[i], t->appended_part_size[i],
                        t->partition_secs_per_head, t->partition_heads_per_cyl,
                        buf, 0);
        }
        if (ret < 0)
            return ret;
    }

    return ISO_SUCCESS;
}

/* Choose *heads_per_cyl so that
   - *heads_per_cyl * secs_per_head * 1024 >= imgsize / 512
   - *heads_per_cyl * secs_per_head is divisible by 4
   - it is as small as possible (to reduce aligment overhead)
   - it is <= 255
   @return 1= success , 0= cannot achieve goals
*/
static
int try_sph(off_t imgsize, int secs_per_head, int *heads_per_cyl, int flag)
{
    off_t hd_blocks, hpc;

    hd_blocks= imgsize / 512;
    hpc = hd_blocks / secs_per_head / 1024;
    if (hpc * secs_per_head * 1024 < hd_blocks)
        hpc++;
    if ((secs_per_head % 4) == 0) {
        ;
    } else if ((secs_per_head % 2) == 0) {
        hpc += (hpc % 2);
    } else if(hpc % 4) {
        hpc += 4 - (hpc % 4);
    }
    if (hpc > 255)
        return 0;
    *heads_per_cyl = hpc;
    return 1;
}

int iso_align_isohybrid(Ecma119Image *t, int flag)
{
    int sa_type, ret, always_align;
    uint32_t img_blocks;
    off_t imgsize, cylsize = 0, frac;

    sa_type = (t->system_area_options >> 2) & 0x3f;
    if (sa_type != 0)
        return ISO_SUCCESS;
    always_align = (t->system_area_options >> 8) & 3;
    if (always_align >= 2)
        return ISO_SUCCESS;

    img_blocks = t->curblock;
    imgsize = ((off_t) img_blocks) * (off_t) 2048;
    if (((t->system_area_options & 3) || always_align)
        && (off_t) (t->partition_heads_per_cyl * t->partition_secs_per_head
                    * 1024) * (off_t) 512 < imgsize) {
        /* Choose small values which can represent the image size */
        /* First try 32 sectors per head */
        ret = try_sph(imgsize, 32, &(t->partition_heads_per_cyl), 0);
        if (ret == 1) {
            t->partition_secs_per_head = 32;
        } else {
            /* Did not work with 32. Try 63 */
            t->partition_secs_per_head = 63;
            ret = try_sph(imgsize, 63, &(t->partition_heads_per_cyl), 0);
            if (ret != 1)
                t->partition_heads_per_cyl = 255;
        }
        cylsize = t->partition_heads_per_cyl * t->partition_secs_per_head *512;
        frac = imgsize % cylsize;
        iso_msg_debug(t->image->id,
                      "Automatically adjusted MBR geometry to %d/%d/%d",
                      (int) (imgsize / cylsize + !!frac),
                      t->partition_heads_per_cyl, t->partition_secs_per_head);
    }

    cylsize = 0;
    if (t->catalog != NULL &&
               (t->catalog->bootimages[0]->isolinux_options & 0x0a) == 0x02) {
        /* Check for isolinux image with magic number of 3.72 and produce
           an MBR from our built-in template. (Deprecated since 31 Mar 2010)
        */
        if (img_blocks >= 0x40000000)
            return ISO_SUCCESS;
        cylsize = 64 * 32 * 512;
    } else if ((t->system_area_options & 2) || always_align) {
        /* Patch externally provided system area as isohybrid MBR */
        if (t->catalog == NULL || t->system_area_data == NULL) {
            /* isohybrid makes only sense together with ISOLINUX boot image
               and externally provided System Area.
            */
            return ISO_ISOLINUX_CANT_PATCH;
        }
        cylsize = t->partition_heads_per_cyl * t->partition_secs_per_head
                  * 512;
    } 
    if (cylsize == 0)
        return ISO_SUCCESS;

    frac = imgsize % cylsize;
    imgsize += (frac > 0 ? cylsize - frac : 0);

    frac = imgsize - ((off_t) img_blocks) * (off_t) 2048;
    if (frac == 0)
        return ISO_SUCCESS;
    t->tail_blocks += frac / 2048 + !!(frac % 2048);
    return ISO_SUCCESS;
}
