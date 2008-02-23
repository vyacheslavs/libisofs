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

/* TODO consider removing this header */

/*
 * Some functions here will be moved to libisofs.h when we expose 
 * Streams.
 */

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

/**
 * Create a stream for reading from a arbitrary memory buffer.
 * When the Stream refcount reach 0, the buffer is free(3).
 * 
 * @return
 *      1 sucess, < 0 error
 */
int iso_memory_stream_new(unsigned char *buf, size_t size, IsoStream **stream);

#endif /*STREAM_H_*/
