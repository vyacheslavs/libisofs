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
#include "fsource.h"

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
     * TODO it would be great to return a different success code if the
     * underlying source size has changed.
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
     * Get the size (in bytes) of the stream. This function should always 
     * return the same size, even if the underlying source size changes.
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
     * 
     * @return
     *     1 if stream is repeatable, 0 if not, < 0 on error
     */
    int (*is_repeatable)(IsoStream *stream);
    
    /**
     * Get an unique identifier for the IsoStream. If your implementation
     * is unable to return a valid identifier, this function should return
     * 0.
     * 
     * @return
     *      1 on success, 0 if idenfier is not valid, < 0 error 
     */
    int (*get_id)(IsoStream *stream, unsigned int *fs_id, dev_t *dev_id, 
                  ino_t *ino_id);

    /**
     * Free implementation specific data. Should never be called by user.
     * Use iso_stream_unref() instead.
     */
    void (*free)(IsoStream *stream);

    int refcount;
    void *data;
};

void iso_stream_ref(IsoStream *stream);
void iso_stream_unref(IsoStream *stream);

/**
 * Create a stream to read from a IsoFileSource.
 * The stream will take the ref. to the IsoFileSource, so after a successfully
 * exectution of this function, you musn't unref() the source, unless you
 * take an extra ref.
 * 
 * @return
 *      1 sucess, < 0 error
 *      Possible errors:
 * 
 */
int iso_file_source_stream_new(IsoFileSource *src, IsoStream **stream);

#endif /*STREAM_H_*/
