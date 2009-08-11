/*
 * Copyright (c) 2009 Thomas Schmitt
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_MD5_H_
#define LIBISO_MD5_H_


/* The MD5 computation API is in libisofs.h : iso_md5_start() et.al. */


/** Create a writer object for checksums and add it to the writer list of
    the given Ecma119Image.
*/
int checksum_writer_create(Ecma119Image *target);


/* Function to identify and manage md5sum indice of the old image.
 * data is supposed to be a 4 byte integer, bit 31 shall be 0,
 * value 0 of this integer means that it is not a valid index.
 */
int checksum_xinfo_func(void *data, int flag);


#endif /* ! LIBISO_MD5_H_ */


