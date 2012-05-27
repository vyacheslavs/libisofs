
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <ctype.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* for gettimeofday() */
#include <sys/time.h>

#include "filesrc.h"
#include "ecma119.h"
#include "eltorito.h"


/* This code stems from syslinux-3.72/utils/isohybrid, a perl script
under GPL which is Copyright 2002-2008 H. Peter Anvin.

Line numbers in comments refer to the lines of that script.
It has been analyzed and re-written in C language 2008 by Thomas Schmitt,
and is now under the licenses to which H.Peter Anvin agreed:

 http://syslinux.zytor.com/archives/2008-November/011105.html
 Date: Mon, 10 Nov 2008 08:36:46 -0800
 From: H. Peter Anvin <hpa@zytor.com>
 I hereby give permission for this code, translated to the C language, to
 be released under either the LGPL or the MIT/ISC/2-clause BSD licenses
 or both, at your option.
 Sincerely, H. Peter Anvin

In the context of GNU xorriso, this code is under GPLv3+ derived from LGPL.
In the context of libisofs this code derives its matching free software
license from above stem licenses, typically from LGPL.
In case its generosity is needed, here is the 2-clause BSD license:

make_isohybrid_mbr.c is copyright 2002-2008 H. Peter Anvin
                              and 2008-2012 Thomas Schmitt

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED `AS IS' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE PROVIDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


/* A helper function. One could replace it by one or two macros. */
static int lsb_to_buf(char **wpt, uint32_t value, int bits, int flag)
{
    int b;

    for (b = 0; b < bits; b += 8)
        *((unsigned char *) ((*wpt)++)) = (value >> b) & 0xff;
    return (1);
}


/* ====================================================================== */
/*                          Deprecated Function                           */
/* ====================================================================== */

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
int make_isohybrid_mbr(int bin_lba, int *img_blocks, char *mbr, int flag)
{
    /* According to H. Peter Anvin this binary code stems from
     syslinux-3.72/mbr/isohdpfx.S
     */
    static int mbr_code_size = 271;
    static unsigned char mbr_code[271] = { 0xfa, 0x31, 0xc0, 0x8e, 0xd8, 0x8e,
            0xd0, 0xbc, 0x00, 0x7c, 0x89, 0xe6, 0x06, 0x57, 0x52, 0x8e, 0xc0,
            0xfb, 0xfc, 0xbf, 0x00, 0x06, 0xb9, 0x00, 0x01, 0xf3, 0xa5, 0xea,
            0x20, 0x06, 0x00, 0x00, 0x52, 0xb4, 0x41, 0xbb, 0xaa, 0x55, 0x31,
            0xc9, 0x30, 0xf6, 0xf9, 0xcd, 0x13, 0x72, 0x14, 0x81, 0xfb, 0x55,
            0xaa, 0x75, 0x0e, 0x83, 0xe1, 0x01, 0x74, 0x09, 0x66, 0xc7, 0x06,
            0xb4, 0x06, 0xb4, 0x42, 0xeb, 0x15, 0x5a, 0x51, 0xb4, 0x08, 0xcd,
            0x13, 0x83, 0xe1, 0x3f, 0x51, 0x0f, 0xb6, 0xc6,

            0x40, 0x50, 0xf7, 0xe1, 0x52, 0x50, 0xbb, 0x00, 0x7c, 0xb9, 0x04,
            0x00, 0x66, 0xa1, 0xb0, 0x07, 0xe8, 0x40, 0x00, 0x72, 0x74, 0x66,
            0x40, 0x80, 0xc7, 0x02, 0xe2, 0xf4, 0x66, 0x81, 0x3e, 0x40, 0x7c,
            0xfb, 0xc0, 0x78, 0x70, 0x75, 0x07, 0xfa, 0xbc, 0xf4, 0x7b, 0xe9,
            0xc6, 0x75, 0xe8, 0x79, 0x00, 0x69, 0x73, 0x6f, 0x6c, 0x69, 0x6e,
            0x75, 0x78, 0x2e, 0x62, 0x69, 0x6e, 0x20, 0x6d, 0x69, 0x73, 0x73,
            0x69, 0x6e, 0x67, 0x20, 0x6f, 0x72, 0x20, 0x63, 0x6f, 0x72, 0x72,
            0x75, 0x70, 0x74,

            0x2e, 0x0d, 0x0a, 0x66, 0x60, 0x66, 0x31, 0xd2, 0x66, 0x52, 0x66,
            0x50, 0x06, 0x53, 0x6a, 0x01, 0x6a, 0x10, 0x89, 0xe6, 0x66, 0xf7,
            0x36, 0xf0, 0x7b, 0xc0, 0xe4, 0x06, 0x88, 0xe1, 0x88, 0xc5, 0x92,
            0xf6, 0x36, 0xf6, 0x7b, 0x88, 0xc6, 0x08, 0xe1, 0x41, 0xb8, 0x01,
            0x02, 0x8a, 0x16, 0xfa, 0x7b, 0xcd, 0x13, 0x8d, 0x64, 0x10, 0x66,
            0x61, 0xc3, 0xe8, 0x1e, 0x00, 0x4f, 0x70, 0x65, 0x72, 0x61, 0x74,
            0x69, 0x6e, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x20,
            0x6c, 0x6f, 0x61,

            0x64, 0x20, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x2e, 0x0d, 0x0a, 0x5e,
            0xac, 0xb4, 0x0e, 0x8a, 0x3e, 0x62, 0x04, 0xb3, 0x07, 0xcd, 0x10,
            0x3c, 0x0a, 0x75, 0xf1, 0xcd, 0x18, 0xf4, 0xeb, 0xfd };

    static int h = 64, s = 32;

    int i, id;
    char *wpt;
    off_t imgsize, cylsize, frac, padding, c, cc;

    /* For generating a weak random number */
    struct timeval tv;
    struct timezone tz;

    if (bin_lba < 0 || bin_lba >= (1 << 29))
        return (0); /* 1 TB limit of signed 32 bit addressing of 512 byte blocks */

    /*
     84: Gets size of image in bytes.
     89:
     */
    imgsize = ((off_t) *img_blocks) * (off_t) 2048;

    /*
     90: Computes $padding, size of padded image and number $c of
     pseudo-cylinders ($h and $s are constants set in line 24).
     102: $cc is $c curbed to 1024.
     */
    cylsize = h * s * 512;
    frac = imgsize % cylsize;
    padding = (frac > 0 ? cylsize - frac : 0);
    imgsize += padding;
    *img_blocks = imgsize / (off_t) 2048;
    c = imgsize / cylsize;
    if (c > 1024) {
        cc = 1024;
    } else
        cc = c;

    /*
     107: Brings the hex-encoded bytes from the file __END__ into
     113: the MBR buffer. (This function holds them in mbr_code[].)
     */
    for (i = 0; i < mbr_code_size; i++)
        mbr[i] = mbr_code[i];

    /*
     118: Pads up to 432
     */
    for (i = mbr_code_size; i < 432; i++)
        mbr[i] = 0;

    /* From here on use write cursor wpt */
    wpt = mbr + 432;

    /*
     120: Converts LBA of isolinux.bin to blocksize 512 and writes
     to buffer. Appends 4 zero bytes.
     */
    lsb_to_buf(&wpt, bin_lba * 4, 32, 0);
    lsb_to_buf(&wpt, 0, 32, 0);

    /*
     121: I do not understand where $id should eventually come
     from. An environment variable ?
     125: Whatever, i use some 32-bit random value with no crypto strength.
     */
    gettimeofday(&tv, &tz);
    id = 0xffffffff & (tv.tv_sec ^ (tv.tv_usec * 2000));

    /*
     126: Adds 4 id bytes and 2 zero bytes.
     */
    lsb_to_buf(&wpt, id, 32, 0);
    lsb_to_buf(&wpt, 0, 16, 0);

    /*
     129: Composes 16 byte record from the parameters determined
     147: so far. Appends it to buffer and add 48 zero bytes.
     */
    /* There are 4 "partition slots". Only the first is non-zero here.
     In the perl script, the fields are set in variables and then
     written to buffer. I omit the variables.
     */
    /* 0x80 */
    lsb_to_buf(&wpt, 0x80, 8, 0);
    /* bhead */
    lsb_to_buf(&wpt, 0, 8, 0);
    /* bsect */
    lsb_to_buf(&wpt, 1, 8, 0);
    /* bcyl */
    lsb_to_buf(&wpt, 0, 8, 0);
    /* fstype */
    lsb_to_buf(&wpt, 0x83, 8, 0);
    /* ehead */
    lsb_to_buf(&wpt, h - 1, 8, 0);
    /* esect */
    lsb_to_buf(&wpt, s + (((cc - 1) & 0x300) >> 2), 8, 0);
    /* ecyl */
    lsb_to_buf(&wpt, (cc - 1) & 0xff, 8, 0);
    /* 0 */
    lsb_to_buf(&wpt, 0, 32, 0);
    /* psize */
    lsb_to_buf(&wpt, c * h * s, 32, 0);

    /* Fill the other three slots with zeros */
    for (i = 0; i < 3 * 4; i++)
        lsb_to_buf(&wpt, 0, 32, 0);

    /*
     148: Appends bytes 0x55 , 0xAA.
     */
    lsb_to_buf(&wpt, 0x55, 8, 0);
    lsb_to_buf(&wpt, 0xaa, 8, 0);

    return (1);
}


/* ====================================================================== */
/*                          The New MBR Producer                          */
/* ====================================================================== */

/* The new MBR producer for isohybrid is a slightly generalized version of
   the deprecated function make_isohybrid_mbr(). It complies to the urge
   of H.Peter Anvin not to hardcode MBR templates but rather to read a
   file from the Syslinux tree, and to patch it as was done with the old
   MBR producer.

   The old algorithm was clarified publicly by the following mail.
   Changes towards the old algorithm:
   - 512-byte LBA of boot image is extended to 64 bit (we stay with 32)
   - check for a magic number is now gone

   The new implementation tries to use similar terms as the mail in order
   to facilitate its future discussion with Syslinux developers.

From hpa@zytor.com Thu Apr  1 08:32:52 2010
Date: Wed, 31 Mar 2010 14:53:51 -0700
From: H. Peter Anvin <hpa@zytor.com>
To: For discussion of Syslinux and tftp-hpa <syslinux@zytor.com>
Cc: Thomas Schmitt <scdbackup@gmx.net>
Subject: Re: [syslinux] port syslinux isohybrid perl script to C

[...]

[me:]
> Currently i lack of blob and prescriptions.

The blobs are available in the Syslinux build tree under the names:

mbr/isohdp[fp]x*.bin

The default probably should be mbr/isohdppx.bin, but it's ultimately up
to the user.

User definable parameters:

-> MBR ID		(default random 32-bit number,
			 or preserved from previous instance)
-> Sector count		(default 32, range 1-63)
-> Head count		(default 64, range 1-256)
-> Partition offset	(default 0, range 0-64)
-> Partition number	(default 1, range 1-4)
-> Filesystem type	(default 0x17, range 1-255)

Note: the filesystem type is largely arbitrary, in theory it can be any
value other than 0x00, 0x05, 0x0f, 0x85, 0xee, or 0xef.  0x17 ("Windows
IFS Hidden") seems safeish, some people believe 0x83 (Linux) is better.

Here is the prescriptions for how to install it:

All numbers are littleendian.  "word" means 16 bits, "dword" means 32
bits, "qword" means 64 bits.

Common subroutine LBA_to_CHS():
	s = (lba % sector_count) + 1
	t = (lba / sector_count)
	h = (t % head_count)
	c = (t / head_count)

	if (c >= 1024):
		c = 1023
		h = head_count
		s = sector_count

	s = s | ((c & 0x300) >> 2)
	c = c & 0xff

	write byte h
	write byte s
	write byte c

Main:
	Pad image_size to a multiple of sector_count*head_count
	Use the input file unmodified for bytes 0..431
	write qword boot_lba		# Offset 432
	write dword mbr_id		# Offset 440
	write word 0			# Offset 444

	# Offset 446
	For each partition entry 1..4:
		if this_partition != partition_number:
			write 16 zero bytes
		else:
			write byte 0x80
			write LBA_to_CHS(partition_offset)
			write byte filesystem_type
			write LBA_to_CHS(image_size-1)
			write dword partition_offset
			write dword image_size

	# Offset 510
	write word 0xaa55

	Use the input file unmodified for bytes 512..32767
	(pad with zero as necessary)

[...]

	-hpa
*/


/* >>> ISOHYBRID : mention the new stuff learned from mjg and isohybrid.c */


static
int lba512chs_to_buf(char **wpt, off_t lba, int head_count, int sector_count)
{
    int s, t, h, c;

    s = (lba % sector_count) + 1;
    t = (lba / sector_count);
    h = (t % head_count);
    c = (t / head_count);
    if (c >= 1024) {
        c = 1023;
        h = head_count; /* >>> not -1 ? Limits head_count to 255 */
        s = sector_count;
    }
    s = s | ((c & 0x300) >> 2);
    c = c & 0xff;
    (*((unsigned char **) wpt))[0] = h;
    (*((unsigned char **) wpt))[1] = s;
    (*((unsigned char **) wpt))[2] = c;
    (*wpt)+= 3;
    return(1); 
}


/* Find out whether GPT and APM are desired */
static int assess_gpt_apm(Ecma119Image *t, int *gpt_count, int gpt_idx[128],
                          int *apm_count)
{
    int i, ilx_opts;

    for (i = 0; i < t->catalog->num_bootimages; i++) {
        ilx_opts = t->catalog->bootimages[i]->isolinux_options;
        if (((ilx_opts >> 2) & 63) == 1 || ((ilx_opts >> 2) & 63) == 2) {
            if (*gpt_count < 128)
                gpt_idx[*gpt_count]= i;
            (*gpt_count)++;
        }
        if (ilx_opts & 256)
            (*apm_count)++;
    }
    if (*apm_count > 6) {
        iso_msgs_submit(0,
                   "Too many entries desired for Apple Partition Map. (max 6)",
                   0, "FAILURE", 0);
        return ISO_ISOLINUX_CANT_PATCH;
    }
    return ISO_SUCCESS;
}


/* Insert APM head into MBR */
static int insert_apm_head(uint8_t *buf, int apm_count)
{
    int i;
    static uint8_t apm_mbr_start[32] = {
        0x33, 0xed, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    static uint8_t apm_head[32] = {
        0x45, 0x52, 0x08, 0x00, 0x00, 0x00, 0x90, 0x90,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    if (apm_count) {
        for (i = 0; i < 32; i++)
            if(buf[i] != apm_mbr_start[i])
        break;
        if (i < 32) {
            iso_msgs_submit(0,
               "MBR template file seems not prepared for Apple Partition Map.",
               0, "FAILURE", 0);
            return ISO_ISOLINUX_CANT_PATCH;
        }
        for (i = 0; i < 32; i++)
            buf[i] = apm_head[i];
    }
    return ISO_SUCCESS;
}


/* Describe the first three GPT boot images as MBR partitions */
static int gpt_images_as_mbr_partitions(Ecma119Image *t, char *wpt,
                                        int gpt_idx[128], int *gpt_cursor)
{
    int ilx_opts;
    off_t hd_blocks;
    static uint8_t dummy_chs[3] = {
        0xfe, 0xff, 0xff, 
    };

    wpt[0] = 0;
    memcpy(wpt + 1, dummy_chs, 3);
    ilx_opts = t->catalog->bootimages[gpt_idx[*gpt_cursor]]->isolinux_options;
    if (((ilx_opts >> 2) & 63) == 2)
        wpt[4] = 0x00; /* HFS gets marked as "Empty" */
    else
        ((unsigned char *) wpt)[4] = 0xef; /* "EFI (FAT-12/16/" */

    memcpy(wpt + 5, dummy_chs, 3);

    /* Start LBA (in 512 blocks) */
    wpt += 8;
    lsb_to_buf(&wpt, t->bootsrc[gpt_idx[*gpt_cursor]]->sections[0].block * 4,
               32, 0);

    /* Number of blocks */
    hd_blocks = t->bootsrc[gpt_idx[*gpt_cursor]]->sections[0].size;
    hd_blocks = hd_blocks / 512 + !!(hd_blocks % 512);
    lsb_to_buf(&wpt, (int) hd_blocks, 32, 0);

    (*gpt_cursor)++;
    return ISO_SUCCESS;
}


static void write_gpt_entry(Ecma119Image *t, char *buf, uint8_t type_guid[16],
                            off_t start_lba, off_t end_lba, uint8_t flags[8],
                            uint8_t name[72])
{
    char *wpt = buf;

    memcpy(wpt, type_guid, 16);
    wpt += 16;
    iso_random_uuid(t, (uint8_t *) wpt);
    wpt += 16;
    lsb_to_buf(&wpt, start_lba & 0xffffffff, 32, 0);
    lsb_to_buf(&wpt, (start_lba >> 32) & 0xffffffff, 32, 0);
    lsb_to_buf(&wpt, end_lba & 0xffffffff, 32, 0);
    lsb_to_buf(&wpt, (end_lba >> 32) & 0xffffffff, 32, 0);
    memcpy(wpt, name, 72);
}


static int write_gpt_array(Ecma119Image *t, char *buf, uint32_t part_start)
{
    static uint8_t basic_data_uuid[16] = {
        0xa2, 0xa0, 0xd0, 0xeb, 0xe5, 0xb9, 0x33, 0x44,
        0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7
    };
    static uint8_t hfs_uuid[16] = {
        0x00, 0x53, 0x46, 0x48, 0x00, 0x00, 0xaa, 0x11,
        0xaa, 0x11, 0x00, 0x30, 0x65, 0x43, 0xec, 0xac
    };
    uint8_t *uuid;
    int i, ilx_opts;
    off_t start_lba, end_lba;

    /* >>> First entry describes overall image , basic_data_uuid
    start_lba = ;
    end_lba = ;
    write_gpt_entry(t, buf + 512 * part_start, basic_data_uuid,
                           off_t start_lba, off_t end_lba, uint8_t flags[8],
                           uint8_t name[72])
    */;

    /* >>> Each marked boot image gets its entry */;
    for (i= 0; i < t->catalog->num_bootimages; i++) {
        ilx_opts= (t->catalog->bootimages[i]->isolinux_options >> 2) & 63;
        if (ilx_opts == 1)
            uuid = basic_data_uuid;
        else if (ilx_opts == 1)
            uuid = hfs_uuid;
        else
    continue;

        /* >>> */;

    }

    /* >>> */;

    return ISO_SUCCESS;
}


static int write_gpt_header_block(Ecma119Image *t, char *buf,
                                  uint32_t part_start, uint32_t p_arr_crc)
{
    static char *sig = "EFI PART";
    static char revision[4] = {0x00, 0x00, 0x01, 0x00};
    char *wpt;
    uint32_t crc;
    off_t back_lba;

    memset(buf, 0, 512);
    wpt = buf;

    memcpy(wpt, sig, 8); /* no trailing 0 */
    wpt += 8;
    memcpy(wpt, revision, 4);
    wpt += 4;
    lsb_to_buf(&wpt, 92, 32, 0);

    /* CRC will be inserted later */
    wpt += 4;

    /* reserved */
    lsb_to_buf(&wpt, 0, 32, 0);
    /* Own LBA low 32 */
    lsb_to_buf(&wpt, 1, 32, 0);
    /* Own LBA high 32 */
    lsb_to_buf(&wpt, 0, 32, 0);

    /* Backup LBA is 1 hd block before image end */
    back_lba = t->curblock * 4 - 1;
    lsb_to_buf(&wpt, (uint32_t) (back_lba & 0xffffffff), 32, 1);
    lsb_to_buf(&wpt, (uint32_t) (back_lba >> 32), 32, 1);

    /* First usable LBA for partitions (entry array occupies 32 hd blocks) */
    lsb_to_buf(&wpt, part_start + 32, 32, 0);
    lsb_to_buf(&wpt, 0, 32, 0);

    /* Last usable LBA for partitions */
    lsb_to_buf(&wpt, (uint32_t) ((back_lba - 32) & 0xffffffff), 32, 1);
    lsb_to_buf(&wpt, (uint32_t) ((back_lba - 32) >> 32), 32, 1);

    /* Disk GUID */
    iso_random_uuid(t, (uint8_t *) wpt);
    wpt += 16;

    /* Partition entries start */
    lsb_to_buf(&wpt, part_start, 32, 0);
    lsb_to_buf(&wpt, 0, 32, 0);

    /* Number of partition entries */
    lsb_to_buf(&wpt, 128, 32, 0);

    /* Size of a partition entry */
    lsb_to_buf(&wpt, 128, 32, 0);

    /* CRC-32 of the partition array */
    lsb_to_buf(&wpt, p_arr_crc, 32, 0);


    /* <<< Only for a first test */
    if (wpt - buf != 92) {
        iso_msgs_submit(0,
                   "program error : write_gpt_header_block : wpt != 92",
                   0, "FATAL", 0);
        return ISO_ISOLINUX_CANT_PATCH;
    }


    /* CRC-32 of this header while head_crc is 0 */
    wpt = buf + 16;
    crc = iso_crc32_gpt((unsigned char *) buf, wpt - buf, 0); 
    lsb_to_buf(&wpt, crc, 32, 0);

    return ISO_SUCCESS;
}


/*
 * @param flag  bit0= make own random MBR Id from current time
 */
int make_isolinux_mbr(int32_t *img_blocks, Ecma119Image *t,
                      int part_offset, int part_number, int fs_type,
                      uint8_t *buf, int flag)
{
    uint32_t id, part, nominal_part_size;
    off_t hd_img_blocks, hd_boot_lba;
    char *wpt;
    uint32_t boot_lba, mbr_id, p_arr_crc, part_start;
    int head_count, sector_count, ret;
    int gpt_count = 0, gpt_idx[128], apm_count = 0, gpt_cursor;
    /* For generating a weak random number */
    struct timeval tv;
    struct timezone tz;

    hd_img_blocks = ((off_t) *img_blocks) * (off_t) 4;

    boot_lba = t->bootsrc[0]->sections[0].block;
    mbr_id = 0;
    head_count = t->partition_heads_per_cyl;
    sector_count = t->partition_secs_per_head;

    ret = assess_gpt_apm(t, &gpt_count, gpt_idx, &apm_count);
    if (ret < 0)
        return ret;
    ret = insert_apm_head(buf, apm_count);
    if (ret < 0)
        return ret;


    /* Padding of image_size to a multiple of sector_count*head_count
       happens already at compute time and is implemented by
       an appropriate increase of Ecma119Image->tail_blocks.
    */

    if (gpt_count) {

        /* >>> ISOHYBRID : need to make sure that backup GPT fits into tail */;

    }

    wpt = (char *) buf + 432;

    /* write qword boot_lba            # Offset 432
    */
    hd_boot_lba = ((off_t) boot_lba) * (off_t) 4;
    lsb_to_buf(&wpt, hd_boot_lba & 0xffffffff, 32, 0);
    lsb_to_buf(&wpt, hd_boot_lba >> 32, 32, 0);

    /* write dword mbr_id              # Offset 440 
       (here some 32-bit random value with no crypto strength)
    */
    if (flag & 1) {
        gettimeofday(&tv, &tz);
        id = 0xffffffff & (tv.tv_sec ^ (tv.tv_usec * 2000));
        lsb_to_buf(&wpt, id, 32, 0);
    }

    /* write word 0                    # Offset 444
    */
    lsb_to_buf(&wpt, 0, 16, 0);

    /* # Offset 446
    */
    gpt_cursor= 0;
    for (part = 1 ; part <= 4; part++) {
        if ((int) part != part_number) {
            /* if this_partition != partition_number: write 16 zero bytes
               (this is now overriden by the eventual desire to announce
                EFI and HFS boot images.)
            */
            memset(wpt, 0, 16);

            if (gpt_cursor < gpt_count) {
                ret = gpt_images_as_mbr_partitions(t, wpt, gpt_idx,
                                                   &gpt_cursor);
                if (ret < 0)
                    return ret;
            }
            wpt+= 16;
    continue;
        }
        /* write byte 0x80
           write LBA_to_CHS(partition_offset)
           write byte filesystem_type
           write LBA_to_CHS(image_size-1)
           write dword partition_offset
           write dword image_size
        */
        lsb_to_buf(&wpt, 0x80, 8, 0);
        lba512chs_to_buf(&wpt, part_offset, head_count, sector_count);
        lsb_to_buf(&wpt, fs_type, 8, 0);
        lba512chs_to_buf(&wpt, hd_img_blocks - 1, head_count, sector_count);
        lsb_to_buf(&wpt, part_offset, 32, 0);
        if (hd_img_blocks - (off_t) part_offset > (off_t) 0xffffffff)
            nominal_part_size = 0xffffffff;
        else
            nominal_part_size = hd_img_blocks - (off_t) part_offset;
        lsb_to_buf(&wpt, nominal_part_size, 32, 0);
    }

    /*  write word 0xaa55            # Offset 510
    */
    lsb_to_buf(&wpt, 0xaa55, 16, 0);

    if (gpt_count) {

        /* >>> ISOHYBRID : write primary GPT and compute p_arr_crc */;
        part_start = 4 + (apm_count + 1) * 4;


        ret = write_gpt_header_block(t, (char *) buf + 512, 
                                     part_start, p_arr_crc);
        if (ret < 0)
            return ret;
    }

    if (apm_count) {

        /* >>> ISOHYBRID : write APM entry blocks (2K) */;

    }

    return(1);
}


