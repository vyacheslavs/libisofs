/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_STREAM_H_
#define LIBISO_STREAM_H_

/*
 * Definitions of streams.
 */

/*
 * Some functions here will be moved to libisofs.h when we expose 
 * Streams.
 */

typedef struct Iso_Stream IsoStream;

struct Iso_Stream
{
    /**
     * Opens the stream.
     * 
     * @return 
     *     1 on success, < 0 on error
     */
    int (*open)(IsoStream *stream);

    /**
     * Close the Stream.
     * @return 1 on success, < 0 on error
     */
    int (*close)(IsoStream *stream);

    /**
     * Get the size (in bytes) of the stream.
     */
    off_t (*get_size)(IsoStream *stream);

    /**
     * Attempts to read up to count bytes from the given stream into
     * the buffer starting at buf.
     * 
     * The stream must be open() before calling this, and close() when no 
     * more needed. 
     * 
     * @return 
     *     number of bytes read, 0 if EOF, < 0 on error
     */
    int (*read)(IsoStream *stream, void *buf, size_t count);
    
    /**
     * Whether this Stram can be read several times, with the same results. 
     * For example, a regular file is repeatable, you can read it as many
     * times as you want. However, a pipe isn't. 
     * 
     * This function doesn't take into account if the file has been modified
     * between the two reads. 
     */
    int (*is_repeatable)(IsoStream *stream);

    int refcount;
    void *data;
};

#endif /*STREAM_H_*/
