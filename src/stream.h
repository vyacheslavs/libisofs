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
	 * @return 1 on success, < 0 on error
	 */
	int (*open)(IsoStream *stream);
	
	/**
	 * Close the Stream.
	 */
	void (*close)(IsoStream *stream);
	
	// Stream should read in 2k blocks!
	//...
	
	int refcount;
	
	
	
}

#endif /*STREAM_H_*/
