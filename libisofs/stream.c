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
#include "util.h"

#include <stdlib.h>
#include <string.h>

ino_t serial_id = (ino_t)1;
ino_t mem_serial_id = (ino_t)1;

typedef struct
{
    IsoFileSource *src;

    /* key for file identification inside filesystem */
    dev_t dev_id;
    ino_t ino_id;
    off_t size; /**< size of this file */
} FSrcStreamData;

static
int fsrc_open(IsoStream *stream)
{
    IsoFileSource *src;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = ((FSrcStreamData*)stream->data)->src;
    return iso_file_source_open(src);
}

static
int fsrc_close(IsoStream *stream)
{
    IsoFileSource *src;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = ((FSrcStreamData*)stream->data)->src;
    return iso_file_source_close(src);
}

static
off_t fsrc_get_size(IsoStream *stream)
{
    FSrcStreamData *data;
    data = (FSrcStreamData*)stream->data;

    return data->size;
}

static
int fsrc_read(IsoStream *stream, void *buf, size_t count)
{
    IsoFileSource *src;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = ((FSrcStreamData*)stream->data)->src;
    return iso_file_source_read(src, buf, count);
}

static
int fsrc_is_repeatable(IsoStream *stream)
{
    int ret;
    struct stat info;
    FSrcStreamData *data;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = (FSrcStreamData*)stream->data;

    /* mode is not cached, this function is only useful for filters */
    ret = iso_file_source_stat(data->src, &info);
    if (ret < 0) {
        return ret;
    }
    if (S_ISREG(info.st_mode) || S_ISBLK(info.st_mode)) {
        return 1;
    } else {
        return 0;
    }
}

static
void fsrc_get_id(IsoStream *stream, unsigned int *fs_id, dev_t *dev_id,
                ino_t *ino_id)
{
    FSrcStreamData *data;
    IsoFilesystem *fs;
    
    data = (FSrcStreamData*)stream->data;
    fs = iso_file_source_get_filesystem(data->src);

    *fs_id = fs->get_id(fs);
    *dev_id = data->dev_id;
    *ino_id = data->ino_id;
}

static
char *fsrc_get_name(IsoStream *stream)
{
    FSrcStreamData *data;
    data = (FSrcStreamData*)stream->data;
    return iso_file_source_get_path(data->src);
}

static
void fsrc_free(IsoStream *stream)
{
    FSrcStreamData *data;
    data = (FSrcStreamData*)stream->data;
    iso_file_source_unref(data->src);
    free(data);
}

IsoStreamIface fsrc_stream_class = {
    fsrc_open,
    fsrc_close,
    fsrc_get_size,
    fsrc_read,
    fsrc_is_repeatable,
    fsrc_get_id,
    fsrc_get_name,
    fsrc_free
};

int iso_file_source_stream_new(IsoFileSource *src, IsoStream **stream)
{
    int r;
    struct stat info;
    IsoStream *str;
    FSrcStreamData *data;

    if (src == NULL || stream == NULL) {
        return ISO_NULL_POINTER;
    }

    r = iso_file_source_stat(src, &info);
    if (r < 0) {
        return r;
    }
    if (S_ISDIR(info.st_mode)) {
        return ISO_FILE_IS_DIR;
    }
    
    /* check for read access to contents */
    r = iso_file_source_access(src);
    if (r < 0) {
        return r;
    }

    str = malloc(sizeof(IsoStream));
    if (str == NULL) {
        return ISO_OUT_OF_MEM;
    }
    data = malloc(sizeof(FSrcStreamData));
    if (str == NULL) {
        free(str);
        return ISO_OUT_OF_MEM;
    }

    /* take the ref to IsoFileSource */
    data->src = src;
    data->size = info.st_size;
    
    /* get the id numbers */
    {
        IsoFilesystem *fs;
        unsigned int fs_id;
        fs = iso_file_source_get_filesystem(data->src);

        fs_id = fs->get_id(fs);
        if (fs_id == 0) {
            /* 
             * the filesystem implementation is unable to provide valid
             * st_dev and st_ino fields. Use serial_id.
             */
            data->dev_id = (dev_t) 0;
            data->ino_id = serial_id++;
        } else {
            data->dev_id = info.st_dev;
            data->ino_id = info.st_ino;
        }
    }

    str->refcount = 1;
    str->data = data;
    str->class = &fsrc_stream_class;

    *stream = str;
    return ISO_SUCCESS;
}



typedef struct
{
    uint8_t *buf;
    ssize_t offset; /* -1 if stream closed */
    ino_t ino_id;
    size_t size;
} MemStreamData;

static
int mem_open(IsoStream *stream)
{
    MemStreamData *data;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = (MemStreamData*)stream->data;
    if (data->offset != -1) {
        return ISO_FILE_ALREADY_OPENNED;
    }
    data->offset = 0;
    return ISO_SUCCESS;
}

static
int mem_close(IsoStream *stream)
{
    MemStreamData *data;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = (MemStreamData*)stream->data;
    if (data->offset == -1) {
        return ISO_FILE_NOT_OPENNED;
    }
    data->offset = -1;
    return ISO_SUCCESS;
}

static
off_t mem_get_size(IsoStream *stream)
{
    MemStreamData *data;
    data = (MemStreamData*)stream->data;

    return (off_t)data->size;
}

static
int mem_read(IsoStream *stream, void *buf, size_t count)
{
    size_t len;
    MemStreamData *data;
    if (stream == NULL || buf == NULL) {
        return ISO_NULL_POINTER;
    }
    if (count == 0) {
        return ISO_WRONG_ARG_VALUE;
    }
    data = stream->data;
    
    if (data->offset == -1) {
        return ISO_FILE_NOT_OPENNED;
    }
    
    len = MIN(count, data->size - data->offset);
    memcpy(buf, data->buf + data->offset, len);
    return len;
}

static
int mem_is_repeatable(IsoStream *stream)
{
    return 1;
}

static
void mem_get_id(IsoStream *stream, unsigned int *fs_id, dev_t *dev_id,
                ino_t *ino_id)
{
    MemStreamData *data;
    data = (MemStreamData*)stream->data;
    *fs_id = ISO_MEM_FS_ID;
    *dev_id = 0;
    *ino_id = data->ino_id;
}

static
char *mem_get_name(IsoStream *stream)
{
    return strdup("[MEMORY SOURCE]");
}

static
void mem_free(IsoStream *stream)
{
    MemStreamData *data;
    data = (MemStreamData*)stream->data;
    free(data->buf);
    free(data);
}

IsoStreamIface mem_stream_class = {
    mem_open,
    mem_close,
    mem_get_size,
    mem_read,
    mem_is_repeatable,
    mem_get_id,
    mem_get_name,
    mem_free
};

/**
 * Create a stream for reading from a arbitrary memory buffer.
 * When the Stream refcount reach 0, the buffer is free(3).
 * 
 * @return
 *      1 sucess, < 0 error
 */
int iso_memory_stream_new(unsigned char *buf, size_t size, IsoStream **stream)
{
    IsoStream *str;
    MemStreamData *data;

    if (buf == NULL || stream == NULL) {
        return ISO_NULL_POINTER;
    }

    str = malloc(sizeof(IsoStream));
    if (str == NULL) {
        return ISO_OUT_OF_MEM;
    }
    data = malloc(sizeof(MemStreamData));
    if (str == NULL) {
        free(str);
        return ISO_OUT_OF_MEM;
    }

    /* fill data */
    data->buf = buf;
    data->size = size;
    data->offset = -1;
    data->ino_id = mem_serial_id++;

    str->refcount = 1;
    str->data = data;
    str->class = &mem_stream_class;

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
        stream->class->free(stream);
        free(stream);
    }
}

inline
int iso_stream_open(IsoStream *stream)
{
    return stream->class->open(stream);
}

inline
int iso_stream_close(IsoStream *stream)
{
    return stream->class->close(stream);
}

inline
off_t iso_stream_get_size(IsoStream *stream)
{
    return stream->class->get_size(stream);
}

inline
int iso_stream_read(IsoStream *stream, void *buf, size_t count)
{
    return stream->class->read(stream, buf, count);
}

inline
int iso_stream_is_repeatable(IsoStream *stream)
{
    return stream->class->is_repeatable(stream);
}

inline
void iso_stream_get_id(IsoStream *stream, unsigned int *fs_id, dev_t *dev_id,
                      ino_t *ino_id)
{
    stream->class->get_id(stream, fs_id, dev_id, ino_id);
}

inline
char *iso_stream_get_name(IsoStream *stream)
{
    return stream->class->get_name(stream);
}
