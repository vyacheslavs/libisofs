/*
 * Copyright (c) 2009 Thomas Schmitt
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_MD5_H_
#define LIBISO_MD5_H_


/** Compute a MD5 checksum from one or more calls of this function.
    The first call has to be made with flag bit0 == 1. It may already submit
    processing payload in data and datalen.
    The last call has to be made with bit15 set. Normally bit1 will be set
    too in order to receive the checksum before it gets disposed.
    bit1 may only be set in the last call or together with bit2.
    The combination of bit1 and bit2 may be used to get an intermediate
    result without hampering an ongoing checksum computation.

    @param ctx      the checksum context which stores the state between calls.
                    It gets created with flag bit0 and disposed with bit15.
                    With flag bit0, *ctx has to be NULL or point to freeable
                    memory.
    @param data     the bytes to be checksummed
    @param datalen  the number of bytes in data
    @param result   returns the 16 bytes of checksum if called with bit1 set
    @param flag     bit0= allocate and init *ctx
                    bit1= transfer ctx to result
                    bit2= with bit 0 : clone new *ctx from data
                    bit15= free *ctx
*/
int libisofs_md5(void **ctx, char *data, int datalen,
                 char result[16], int flag);


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


