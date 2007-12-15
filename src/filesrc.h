/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_FILESRC_H_
#define LIBISO_FILESRC_H_

#include "libisofs.h"
#include "stream.h"
#include "ecma119.h"

#include <stdint.h>

typedef struct Iso_File_Src IsoFileSrc;

struct Iso_File_Src {
    
    /* key */
    unsigned int fs_id;
    dev_t dev_id;
    ino_t ino_id;
    
    unsigned int prev_img:1; /**< if the file comes from a previous image */
    off_t size; /**< size of this file */
    uint32_t block; /**< Block where this file will be written on image */
    int sort_weight;
    IsoStream *stream;
};

/**
 * Create a new IsoFileSrc to get data from a specific IsoFile.
 * 
 * The IsoFileSrc will be cached in a tree to prevent the same file for 
 * being written several times to image. If you call again this function
 * with a node that refers to the same source file, the previously
 * created one will be returned. No new IsoFileSrc is created in that case.
 * 
 * @param img
 *      The image where this file is to be written
 * @param file
 *      The IsoNode we want to write
 * @param src
 *      Will be filled with a pointer to the IsoFileSrc
 * @return
 *      1 on success, < 0 on error
 */
int iso_file_src_create(Ecma119Image *img, IsoFile *file, IsoFileSrc **src);

// TODO not implemented
int iso_file_src_open(IsoFileSrc *file);

// TODO not implemented
int iso_file_src_close(IsoFileSrc *file);

/**
 * TODO define propertly this comment
 * TODO not implemented
 * 
 * Read a block (2048 bytes) from the IsoFileSrc.
 * 
 * This function should always read the full 2048 bytes, blocking if
 * needed. When it reaches EOF, the buf is filled with 0's, if needed. 
 * Note that the EOF is not reported in that call, but in the next call.
 * I.e., when the EOF is reported you can be sure that the function
 * has not written anything to the buffer. If the file size is a multiple
 * of block size, i.e N*2048, the read_block reports the EOF just when 
 * reading the N+1 block.  
 * 
 * Note that EOF refers to the original size as reported by get_size.
 * If the underlying source size has changed, this function should take 
 * care of this, truncating the file, or filling the buffer with 0s. I.e.
 * this function return 0 (EOF) only when get_size() bytes have been 
 * readed.
 * 
 * On an I/O error, or a file smaller than the expected size, this
 * function returns a [specific error code], and the buffer is filled
 * with 0s. Subsequent calls will still return an error code and fill
 * buffer with 0's, until EOF (as defined above) is reached, and then 
 * the function will return 0. 
 * 
 * Note that if the current size is larger than expected, you don't get 
 * any error on reading.
 * 
 * @param buf 
 *     Buffer where data fill be copied, with at least 2048 bytes.
 * @return
 *     1 sucess, 0 EOF, < 0 error (buf is filled with 0's)
 */
int iso_file_src_read_block(IsoFileSrc *file, void *buf);

#endif /*LIBISO_FILESRC_H_*/
