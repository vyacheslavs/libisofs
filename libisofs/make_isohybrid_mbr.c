#include <ctype.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* for gettimeofday() */
#include <sys/time.h>

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

In the context of xorriso-standalone, this code is under GPLv2 derived from
LGPL. In the context of libisofs this code derives its matching open source
license from above stem licenses, typically from LGPL.
In case its generosity is needed, here is the 2-clause BSD license:

make_isohybrid_mbr.c is copyright 2002-2008 H. Peter Anvin
                              and 2008-2009 libburnia project.

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
static int lsb_to_buf(char **wpt, int value, int bits, int flag)
{
    int b;

    for (b = 0; b < bits; b += 8)
        *((unsigned char *) ((*wpt)++)) = (value >> b) & 0xff;
    return (1);
}

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

    int i, warn_size = 0, id;
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
        warn_size = 1;
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

