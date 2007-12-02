/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "libisofs.h"
#include "stream.h"
#include "fsource.h"
#include "error.h"

#include <stdlib.h>

typedef struct
{
    IsoFileSource *src;
} _FSrcStream;

static
int fsrc_open(IsoStream *stream)
{
    IsoFileSource *src;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = (IsoFileSource*)stream->data;
    return src->open(src);
}

static
int fsrc_close(IsoStream *stream)
{
    IsoFileSource *src;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = (IsoFileSource*)stream->data;
    return src->close(src);
}

static
off_t fsrc_get_size(IsoStream *stream)
{
    struct stat info;
    IsoFileSource *src;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = (IsoFileSource*)stream->data;
    
    if (src->lstat(src, &info) < 0) {
        return (off_t) -1;
    }
    
    return info.st_size;
}

static 
int fsrc_read(IsoStream *stream, void *buf, size_t count)
{
    IsoFileSource *src;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = (IsoFileSource*)stream->data;
    return src->read(src, buf, count);
}

static 
int fsrc_is_repeatable(IsoStream *stream)
{
    int ret;
    struct stat info;
    IsoFileSource *src;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = (IsoFileSource*)stream->data;
    
    ret = src->stat(src, &info);
    if (ret < 0) {
        return ret; 
    }
    if (info.st_mode & (S_IFREG | S_IFBLK)) {
        return 1;
    } else {
        return 0;
    }
}

static
void fsrc_free(IsoStream *stream)
{
    IsoFileSource *src;
    src = (IsoFileSource*)stream->data;
    iso_file_source_unref(src);
}

int iso_file_source_stream_new(IsoFileSource *src, IsoStream **stream)
{
    int r;
    struct stat info;
    IsoStream *str;
    if (src == NULL || stream == NULL) {
        return ISO_NULL_POINTER;
    }
    
    r = src->stat(src, &info);
    if (r < 0) {
        return r;
    }
    if (S_ISDIR(info.st_mode)) {
        return ISO_FILE_IS_DIR;
    }
    
    str = malloc(sizeof(IsoStream));
    if (str == NULL) {
        return ISO_MEM_ERROR;
    }
    
    str->refcount = 1;
    str->data = src;
    str->open = fsrc_open;
    str->close = fsrc_close;
    str->get_size = fsrc_get_size;
    str->read = fsrc_read;
    str->is_repeatable = fsrc_is_repeatable;
    str->free = fsrc_free;
    
    *stream = str;
    return ISO_SUCCESS;
}

void iso_stream_ref(IsoStream *stream)
{
    ++stream->refcount;
}

void iso_stream_unref(IsoStream *stream)
{
    if (--stream->refcount == 0) {
        stream->free(stream);
        free(stream);
    }
}
