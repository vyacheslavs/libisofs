/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "fsource.h"
#include <stdlib.h>

/**
 * Values belong 1000 are reserved for libisofs usage
 */
unsigned int iso_fs_global_id = 1000;

void iso_file_source_ref(IsoFileSource *src)
{
    ++src->refcount;
}

void iso_file_source_unref(IsoFileSource *src)
{
    if (--src->refcount == 0) {
        src->class->free(src);
        free(src);
    }
}

void iso_filesystem_ref(IsoFilesystem *fs)
{
    ++fs->refcount;
}

void iso_filesystem_unref(IsoFilesystem *fs)
{
    if (--fs->refcount == 0) {
        fs->free(fs);
        free(fs);
    }
}
