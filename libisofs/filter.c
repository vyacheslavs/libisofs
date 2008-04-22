/*
 * Copyright (c) 2008 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "libisofs.h"
#include "filter.h"
#include "node.h"


void iso_filter_ref(FilterContext *filter)
{
    ++filter->refcount;
}

void iso_filter_unref(FilterContext *filter)
{
    if (--filter->refcount == 0) {
        filter->free(filter);
        free(filter);
    }
}

int iso_file_add_filter(IsoFile *file, FilterContext *filter, int flag)
{
    int ret;
    IsoStream *original, *filtered;
    if (file == NULL || filter == NULL) {
        return ISO_NULL_POINTER;
    }
    
    original = file->stream;

    if (!iso_stream_is_repeatable(original)) {
        /* TODO use custom error */
        return ISO_WRONG_ARG_VALUE;
    }
    
    ret = filter->get_filter(filter, original, &filtered);
    if (ret < 0) {
        return ret;
    }
    iso_stream_unref(original);
    file->stream = filtered;
    return ISO_SUCCESS;
}
