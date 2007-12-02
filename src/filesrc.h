/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_FILESRC_H_
#define LIBISO_FILESRC_H_

/*
 * FIXME definitions here shouldn't be used for now!!!
 * 
 */


/*
 * Definitions of streams.
 */

/*
 * Some functions here will be moved to libisofs.h when we expose 
 * Streams.
 */

//typedef struct Iso_Stream IsoStream;
//
//struct Iso_Stream
//{
//    /**
//     * Opens the stream.
//     * 
//     * @return 
//     *     1 on success, < 0 on error, 2 file is smaller than expect,
//     *     3 file is larger than expected (2 and 3 are not errors, they're
//     *     handled by read_block, see there. Moreover, IsooStream 
//     *     implementations doesn't need to check that)
//     */
//    int (*open)(IsoStream *stream);
//
//    /**
//     * Close the Stream.
//     * @return 1 on success, < 0 on error
//     */
//    int (*close)(IsoStream *stream);
//
//    /**
//     * Get the size (in bytes) of the stream. This function must return
//     * always the same size, even if the underlying source file has changed.
//     */
//    off_t (*get_size)(IsoStream *stream);
//
//    /**
//     * Read a block (2048 bytes) from the stream.
//     * 
//     * This function should always read the full 2048 bytes, blocking if
//     * needed. When it reaches EOF, the buf is filled with 0's, if needed. 
//     * Note that the EOF is not reported in that call, but in the next call.
//     * I.e., when the EOF is reported you can be sure that the function
//     * has not written anything to the buffer. If the file size is a multiple
//     * of block size, i.e N*2048, the read_block reports the EOF just when 
//     * reading the N+1 block.  
//     * 
//     * Note that EOF refers to the original size as reported by get_size.
//     * If the underlying source size has changed, this function should take 
//     * care of this, truncating the file, or filling the buffer with 0s. I.e.
//     * this function return 0 (EOF) only when get_size() bytes have been 
//     * readed.
//     * 
//     * On an I/O error, or a file smaller than the expected size, this
//     * function returns a [specific error code], and the buffer is filled
//     * with 0s. Subsequent calls will still return an error code and fill
//     * buffer with 0's, until EOF (as defined above) is reached, and then 
//     * the function will return 0. 
//     * 
//     * Note that if the current size is larger than expected, you don't get 
//     * any error on reading.
//     * 
//     * @param buf 
//     *     Buffer where data fill be copied, with at least 2048 bytes.
//     * @return
//     *     1 sucess, 0 EOF, < 0 error (buf is filled with 0's)
//     */
//    int (*read_block)(IsoStream *stream, void *buf);
//
//    /**
//     * Whether this Stram can be read several times, with the same results. 
//     * For example, a regular file is repeatable, you can read it as many
//     * times as you want. However, a pipe isn't. 
//     * 
//     * This function doesn't take into account if the file has been modified
//     * between the two reads. 
//     */
//    int (*is_repeatable)(IsoStream *stream);
//
//    int refcount;
//    void *data;
//}

#endif /*LIBISO_FILESRC_H_*/
