/*
 * Copyright (c) 2008 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 */

#include "../libisofs.h"
#include "../filter.h"
#include "../fsource.h"

/*
 * A simple Filter implementation for example purposes. It encrypts a file
 * by XORing each byte by a given key.
 */

static ino_t xor_ino_id = 0;

typedef struct
{
    IsoStream *orig;
    uint8_t key;
    ino_t id;
} XorEncryptStreamData;

static
int xor_encrypt_stream_open(IsoStream *stream)
{
    XorEncryptStreamData *data;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = (XorEncryptStreamData*)stream->data;
    return iso_stream_open(data->orig);
}

static
int xor_encrypt_stream_close(IsoStream *stream)
{
    XorEncryptStreamData *data;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = stream->data;
    return iso_stream_close(data->orig);
}

static
off_t xor_encrypt_stream_get_size(IsoStream *stream)
{
    XorEncryptStreamData *data;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = stream->data;
    return iso_stream_get_size(data->orig);
}

static
int xor_encrypt_stream_read(IsoStream *stream, void *buf, size_t count)
{
    int ret, len;
    XorEncryptStreamData *data;
    uint8_t *buffer = buf;

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = stream->data;
    ret = iso_stream_read(data->orig, buf, count);
    if (ret < 0) {
        return ret;
    }
    
    /* xor */
    for (len = 0; len < ret; ++len) {
        buffer[len] = buffer[len] ^ data->key;
    }
    
    return ret;
}

static
int xor_encrypt_stream_is_repeatable(IsoStream *stream)
{
    /* the filter can't be created if underlying stream is not repeatable */
    return 1;
}

static
void xor_encrypt_stream_get_id(IsoStream *stream, unsigned int *fs_id, 
                               dev_t *dev_id, ino_t *ino_id)
{
    XorEncryptStreamData *data = stream->data;
    *fs_id = ISO_FILTER_FS_ID;
    *dev_id = XOR_ENCRYPT_DEV_ID;
    *ino_id = data->id;
}

static
void xor_encrypt_stream_free(IsoStream *stream)
{
    XorEncryptStreamData *data = stream->data;
    iso_stream_unref(data->orig);
    free(data);
}

IsoStreamIface xor_encrypt_stream_class = {
    0,
    "xorf",
    xor_encrypt_stream_open,
    xor_encrypt_stream_close,
    xor_encrypt_stream_get_size,
    xor_encrypt_stream_read,
    xor_encrypt_stream_is_repeatable,
    xor_encrypt_stream_get_id,
    xor_encrypt_stream_free
};


static
void xor_encrypt_filter_free(FilterContext *filter)
{
    free(filter->data);
}

static
int xor_encrypt_filter_get_filter(FilterContext *filter, IsoStream *original, 
                  IsoStream **filtered)
{
    IsoStream *str;
    XorEncryptStreamData *data;

    if (filter == NULL || original == NULL || filtered == NULL) {
        return ISO_NULL_POINTER;
    }

    str = malloc(sizeof(IsoStream));
    if (str == NULL) {
        return ISO_OUT_OF_MEM;
    }
    data = malloc(sizeof(XorEncryptStreamData));
    if (str == NULL) {
        free(str);
        return ISO_OUT_OF_MEM;
    }

    /* fill data */
    data->key = *((uint8_t*)filter->data);
    data->id = xor_ino_id++;

    /* get reference to the source */
    data->orig = original;
    iso_stream_ref(original);
    
    str->refcount = 1;
    str->data = data;
    str->class = &xor_encrypt_stream_class;

    *filtered = str;
    return ISO_SUCCESS;
}

int create_xor_encrypt_filter(uint8_t key, FilterContext **filter)
{
    FilterContext *f;
    uint8_t *data;
    
    f = calloc(1, sizeof(FilterContext));
    if (f == NULL) {
        return ISO_OUT_OF_MEM;
    }
    data = malloc(sizeof(uint8_t));
    if (data == NULL) {
        free(f);
        return ISO_OUT_OF_MEM;
    }
    f->refcount = 1;
    f->version = 0;
    *data = key;
    f->data = data;
    f->free = xor_encrypt_filter_free;
    f->get_filter = xor_encrypt_filter_get_filter;
    
    return ISO_SUCCESS;
}
