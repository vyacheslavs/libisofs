/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "libisofs.h"
#include "error.h"

#include <stdlib.h>

/**
 * Increments the reference counting of the given IsoDataSource.
 */
void iso_data_source_ref(IsoDataSource *src)
{
    src->refcount++;
}

/**
 * Decrements the reference counting of the given IsoDataSource, freeing it
 * if refcount reach 0.
 */
void iso_data_source_unref(IsoDataSource *src)
{
    if (--src->refcount == 0) {
        src->free_data(src);
        free(src);
    }
}


/**
 * Create a new IsoDataSource from a local file. This is suitable for
 * accessing regular .iso images, or to acces drives via its block device
 * and standard POSIX I/O calls.
 * 
 * @param path
 *     The path of the file
 * @param src
 *     Will be filled with the pointer to the newly created data source.
 * @return
 *    1 on success, < 0 on error.
 */
int iso_data_source_new_from_file(const char *path, IsoDataSource **src)
{
    
    if (path == NULL || src == NULL) {
        return ISO_MEM_ERROR;
    }
    
    //TODO implement
    return -1;
}
