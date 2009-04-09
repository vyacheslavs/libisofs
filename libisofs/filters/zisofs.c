/*
 * Copyright (c) 2009 Thomas Schmitt
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 *
 * It implements a filter facility which can pipe a IsoStream into zisofs
 * compression resp. uncompression, read its output and forward it as IsoStream
 * output to an IsoFile.
 * The zisofs format was invented by H. Peter Anvin. See doc/zisofs_format.txt
 * It is writeable and readable by zisofs-tools, readable by Linux kernels.
 *
 */

#include "../libisofs.h"
#include "../filter.h"
#include "../fsource.h"
#include "../util.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef Libisofs_with_zliB
#include <zlib.h>
#else
/* If zlib is not available then this code is a dummy */
#endif


/*
 * A filter that encodes or decodes the content of zisofs compressed files.
 */


/* --------------------------- ZisofsFilterRuntime ------------------------- */


/* Sizes to be used for compression. Decompression learns from input header. */
#define Libisofs_zisofs_block_log2 15
#define Libisofs_zisofs_block_sizE 32768


/* Individual runtime properties exist only as long as the stream is opened.
 */
typedef struct
{
    int state; /* processing: 0= header, 1= block pointers, 2= data blocks */
    int block_size;

    int block_pointer_fill;
    int block_pointer_rpos;

    char *read_buffer;
    char *block_buffer;
    int buffer_size;
    int buffer_fill;
    int buffer_rpos;

    off_t block_counter;
    off_t in_counter;
    off_t out_counter;

} ZisofsFilterRuntime;


static
int ziso_running_destroy(ZisofsFilterRuntime **running, int flag)
{
    ZisofsFilterRuntime *o= *running;
    if (o == NULL)
        return 0;
    if (o->read_buffer != NULL)
        free(o->read_buffer);
    if (o->block_buffer != NULL)
        free(o->block_buffer);
    free((char *) o);
    *running = NULL;
    return 1;
}


/*
 * @param flag bit0= do not set block_size, do not allocate buffers
 */
static
int ziso_running_new(ZisofsFilterRuntime **running, int flag)
{
    ZisofsFilterRuntime *o;
    *running = o = calloc(sizeof(ZisofsFilterRuntime), 1);
    if (o == NULL) {
        return ISO_OUT_OF_MEM;
    }
    o->state = 0;
    o->block_pointer_fill = 0;
    o->block_pointer_rpos = 0;
    o->block_size= 0;
    o->read_buffer = NULL;
    o->block_buffer = NULL;
    o->buffer_size = 0;
    o->buffer_fill = 0;
    o->buffer_rpos = 0;
    o->block_counter = 0;
    o->in_counter = 0;
    o->out_counter = 0;

    if (flag & 1)
        return 1;

    o->block_size = Libisofs_zisofs_block_sizE;
#ifdef Libisofs_with_zliB
    o->buffer_size= compressBound((uLong) Libisofs_zisofs_block_sizE);
#else
    o->buffer_size= 2 * Libisofs_zisofs_block_sizE;
#endif
    o->read_buffer = calloc(o->block_size, 1);
    o->block_buffer = calloc(o->buffer_size, 1);
    if (o->block_buffer == NULL || o->read_buffer == NULL)
        goto failed;
    return 1;
failed:
    ziso_running_destroy(running, 0);
    return -1;
}


/* ---------------------------- ZisofsFilterStreamData --------------------- */

/* The first 8 bytes of a zisofs compressed data file */
static unsigned char zisofs_magic[9] =
                              {0x37, 0xE4, 0x53, 0x96, 0xC9, 0xDB, 0xD6, 0x07};


/*
 * The data payload of an individual Zisofs Filter IsoStream
 */
typedef struct
{
    ino_t id;

    IsoStream *orig;

    off_t size; /* -1 means that the size is unknown yet */

    uint32_t orig_size;
    uint32_t *block_pointers; /* Cache for output block addresses. They get
                                 written before the data and so need 2 passes.
                                 This cache avoids surplus passes.
                               */

    ZisofsFilterRuntime *running; /* is non-NULL when open */

} ZisofsFilterStreamData;


/* Each individual ZisofsFilterStreamData needs a unique id number. */
/* >>> This is very suboptimal:
       The counter can rollover.
*/
static ino_t ziso_ino_id = 0;


/*
 * Methods for the IsoStreamIface of an External Filter object.
 */


/*
 * @param flag  bit0= original stream is not open
 */
static
int ziso_stream_close_flag(IsoStream *stream, int flag)
{
    ZisofsFilterStreamData *data;

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = stream->data;

    if (data->running == NULL) {
        return 1;
    }
    ziso_running_destroy(&(data->running), 0);
    if (flag & 1)
        return 1;
    return iso_stream_close(data->orig);
}


static
int ziso_stream_close(IsoStream *stream)
{
    return ziso_stream_close_flag(stream, 0);
}


/*
 * @param flag  bit0= do not run .get_size() if size is < 0
 */
static
int ziso_stream_open_flag(IsoStream *stream, int flag)
{
    ZisofsFilterStreamData *data;
    ZisofsFilterRuntime *running = NULL;
    int ret;

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = (ZisofsFilterStreamData*) stream->data;
    if (data->running != NULL) {
        return ISO_FILE_ALREADY_OPENED;
    }
    if (data->size < 0 && !(flag & 1)) {
        /* Do the size determination run now, so that the size gets cached
           and .get_size() will not fail on an opened stream.
        */
        stream->class->get_size(stream);
    }

    ret = ziso_running_new(&running, 0);
    if (ret < 0) {
        return ret;
    }
    data->running = running;

    ret = iso_stream_open(data->orig);
    if (ret < 0) {
        return ret;
    }
    return 1;
}


static
int ziso_stream_open(IsoStream *stream)
{
    return ziso_stream_open_flag(stream, 0);
}


static
int ziso_stream_read(IsoStream *stream, void *buf, size_t desired)
{
    int ret, todo, i;
    ZisofsFilterStreamData *data;
    ZisofsFilterRuntime *rng;
    size_t fill = 0;
    off_t orig_size, next_pt;
    char *cbuf = buf;

#ifdef Libisofs_with_zliB
    uLongf buf_len;
#else
    unsigned long buf_len;
#endif

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = stream->data;
    rng= data->running;
    if (rng == NULL) {
        return ISO_FILE_NOT_OPENED;
    }

    while (1) {
        if (rng->state == 0) {
            /* Delivering file header */

            if (rng->buffer_fill == 0) {
                memcpy(rng->block_buffer, zisofs_magic, 8);
                orig_size = iso_stream_get_size(data->orig);
                if (orig_size > 4294967295.0) {

                    /* >>> cannot compress file >= 4 GiB to zisofs */;
                    /* <<< need better error code (or fallback strategy ?) */
                    return ISO_FILE_READ_ERROR;

                }
                data->orig_size = orig_size;
                iso_lsb((unsigned char *) (rng->block_buffer + 8),
                        (uint32_t) orig_size, 4);
                rng->block_buffer[12] = 4;
                rng->block_buffer[13] = Libisofs_zisofs_block_log2;
                rng->block_buffer[14] = rng->block_buffer[15] = 0;
                rng->buffer_fill = 16;
                rng->buffer_rpos = 0;
            } else if (rng->buffer_rpos >= rng->buffer_fill) {
                rng->buffer_fill = rng->buffer_rpos = 0;
                rng->state = 1; /* header is delivered */
            }
        }
        if (rng->state == 1) {
            /* Delivering block pointers */;

            if (rng->block_pointer_fill == 0) {
                /* Initialize block pointer writing */
                rng->block_pointer_rpos = 0;
                rng->block_pointer_fill = data->orig_size / rng->block_size
                                     + 1 + !!(orig_size % rng->block_size);
                if (data->block_pointers == NULL) {
                    /* On the first pass, create pointer array with all 0s */
                    data->block_pointers = calloc(rng->block_pointer_fill, 4);
                    if (data->block_pointers == NULL) {
                        rng->block_pointer_fill = 0;
                        return ISO_OUT_OF_MEM;
                    }
                }
            }
            if (rng->buffer_rpos >= rng->buffer_fill) {
                if (rng->block_pointer_rpos >= rng->block_pointer_fill) {
                    rng->buffer_fill = rng->buffer_rpos = 0;
                    rng->block_counter = 0;
                    data->block_pointers[0] = 16 + rng->block_pointer_fill * 4;
                    rng->state = 2; /* block pointers are delivered */
                } else {
                    /* Provide a buffer full of block pointers */
                    todo = rng->block_pointer_fill - rng->block_pointer_rpos;
                    if (todo * 4 > rng->buffer_size)
                        todo = rng->buffer_size / 4;
                    memcpy(rng->block_buffer,
                           data->block_pointers + 4 * rng->block_pointer_rpos,
                           todo * 4);
                    rng->buffer_rpos = 0;
                    rng->buffer_fill = todo * 4;
                    rng->block_pointer_rpos += todo;
                }
            }
        }
        if (rng->state == 2 && rng->buffer_rpos >= rng->buffer_fill) {
            /* >>> Delivering data blocks */;

            ret = iso_stream_read(data->orig, rng->read_buffer,
                                  rng->block_size);
            if (ret > 0) {
                rng->in_counter += ret;

                if (rng->in_counter > data->orig_size) {

                    /* >>> Input size overflow */
                    /* <<< need better error code */
                    return ISO_FILE_READ_ERROR;

                }

                /* Check whether all 0 : represent as 0-length block */;
                for (i = 0; i < ret; i++)
                    if (rng->read_buffer[i])
                break;
                if (i >= ret) { /* All 0-bytes. Bypass compression. */
                    buf_len = 0;
                } else {
                    buf_len = rng->buffer_size;

#ifdef Libisofs_with_zliB
                    ret = compress2((Bytef *) rng->block_buffer, &buf_len,
                                   (Bytef *) rng->read_buffer, (uLong) ret, 9);
                    if (ret != Z_OK) {
#else
                    {
#endif
                        /* >>> compression failed */
                        /* <<< need better error code */
                        return ISO_FILE_READ_ERROR;

                    }

                }
                rng->buffer_fill = buf_len;
                rng->buffer_rpos = 0;

                next_pt = data->block_pointers[rng->block_counter] + buf_len;

                /* >>> check for data->size overflow */;
                if (data->size >= 0 && next_pt > data->size) {

                    /* >>> Compression yields more bytes than on first run */
                    /* <<< need better error code */
                    return ISO_FILE_READ_ERROR;

                }

                /* >>> record resp. check block pointer */;
                rng->block_counter++;
                if (data->block_pointers[rng->block_counter] > 0) {
                    /* >>> check */;
                    if (next_pt != data->block_pointers[rng->block_counter] ) {

                        /* >>> block pointers mismatch , content has changed */
                        /* <<< need better error code */
                        return ISO_FILE_READ_ERROR;

                    }
                } else {
                    data->block_pointers[rng->block_counter] = next_pt;
                }

            } else if (ret == 0) {
                rng->state = 3;
                return fill;
            } else
                return ret;
            if (rng->buffer_fill == 0) {
    continue;
            }
        }
        if (rng->state == 3 && rng->buffer_rpos >= rng->buffer_fill) {
            return 0; /* EOF */
        }

        /* Transfer from rng->block_buffer to buf */
        todo = desired - fill;
        if (todo > rng->buffer_fill - rng->buffer_rpos)
            todo = rng->buffer_fill - rng->buffer_rpos;
        memcpy(cbuf + fill, rng->block_buffer + rng->buffer_rpos, todo);
        fill += todo;
        rng->buffer_rpos += todo;
        rng->out_counter += todo;

        if (fill >= desired) {
           return fill;
        }
    }
    return ISO_FILE_READ_ERROR; /* should never be hit */
}


static
off_t ziso_stream_get_size(IsoStream *stream)
{
    int ret, ret_close;
    off_t count = 0;
    ZisofsFilterStreamData *data;
    char buf[64 * 1024];
    size_t bufsize = 64 * 1024;

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = stream->data;

    if (data->size >= 0) {
        return data->size;
    }

    /* Run filter command and count output bytes */
    ret = ziso_stream_open_flag(stream, 1);
    if (ret < 0) {
        return ret;
    }
    while (1) {
        ret = ziso_stream_read(stream, buf, bufsize);
        if (ret <= 0)
            break;
        count += ret;
    }
    ret_close = ziso_stream_close(stream);
    if (ret < 0)
        return ret;
    if (ret_close < 0)
        return ret_close;

    data->size = count;
    return count;
}


static
int ziso_stream_is_repeatable(IsoStream *stream)
{
    /* Only repeatable streams are accepted as orig */
    return 1;
}


static
void ziso_stream_get_id(IsoStream *stream, unsigned int *fs_id, 
                        dev_t *dev_id, ino_t *ino_id)
{
    ZisofsFilterStreamData *data;

    data = stream->data;
    *fs_id = ISO_FILTER_FS_ID;
    *dev_id = ISO_FILTER_ZISOFSL_DEV_ID;
    *ino_id = data->id;
}


static
void ziso_stream_free(IsoStream *stream)
{
    ZisofsFilterStreamData *data;

    if (stream == NULL) {
        return;
    }
    data = stream->data;
    if (data->running != NULL) {
        ziso_stream_close(stream);
    }
    if (data->block_pointers != NULL) {
        free((char *) data->block_pointers);
    }
    iso_stream_unref(data->orig);
    free(data);
}


static
int ziso_update_size(IsoStream *stream)
{
    /* By principle size is determined only once */
    return 1;
}


static
IsoStream *ziso_get_input_stream(IsoStream *stream, int flag)
{
    ZisofsFilterStreamData *data;

    if (stream == NULL) {
        return NULL;
    }
    data = stream->data;
    return data->orig;
}


IsoStreamIface ziso_stream_class = {
    2,
    "ziso",
    ziso_stream_open,
    ziso_stream_close,
    ziso_stream_get_size,
    ziso_stream_read,
    ziso_stream_is_repeatable,
    ziso_stream_get_id,
    ziso_stream_free,
    ziso_update_size,
    ziso_get_input_stream
};


static
void ziso_filter_free(FilterContext *filter)
{
    /* no data are allocated */;
}


/* To be called by iso_file_add_filter().
 * The FilterContext input parameter is not furtherly needed for the 
 * emerging IsoStream.
 */
static
int ziso_filter_get_filter(FilterContext *filter, IsoStream *original, 
                           IsoStream **filtered)
{
    IsoStream *str;
    ZisofsFilterStreamData *data;

    if (filter == NULL || original == NULL || filtered == NULL) {
        return ISO_NULL_POINTER;
    }

    str = calloc(sizeof(IsoStream), 1);
    if (str == NULL) {
        return ISO_OUT_OF_MEM;
    }
    data = calloc(sizeof(ZisofsFilterStreamData), 1);
    if (data == NULL) {
        free(str);
        return ISO_OUT_OF_MEM;
    }

    /* These data items are not owned by this filter object */
    data->id = ++ziso_ino_id;
    data->orig = original;
    data->size = -1;
    data->orig_size = 0;
    data->block_pointers = NULL;
    data->running = NULL;

    /* get reference to the source */
    iso_stream_ref(data->orig);

    str->refcount = 1;
    str->data = data;
    str->class = &ziso_stream_class;

    *filtered = str;

    return ISO_SUCCESS;
}


/* Produce a parameter object suitable for iso_file_add_filter().
 * It may be disposed by free() after all those calls are made.
 *
 * This is quite a dummy as it dows not carry individual data.
 */
static
int ziso_create_context(FilterContext **filter, int flag)
{
    FilterContext *f;
    
    *filter = f = calloc(1, sizeof(FilterContext));
    if (f == NULL) {
        return ISO_OUT_OF_MEM;
    }
    f->refcount = 1;
    f->version = 0;
    f->data = NULL;
    f->free = ziso_filter_free;
    f->get_filter = ziso_filter_get_filter;
    return ISO_SUCCESS;
}


/*
 * @param flag bit0= if_block_reduction rather than if_reduction
 *             >>> bit1= Install a decompression filter
 */
int iso_file_add_zisofs_filter(IsoFile *file, int flag)
{

#ifdef Libisofs_with_zliB

    int ret;
    FilterContext *f = NULL;
    IsoStream *stream;
    off_t original_size = 0, filtered_size = 0;

    original_size = iso_file_get_size(file);
    if (original_size <= 0) {
        return 2;
    }
    if (original_size > 4294967295.0) {
        return ISO_ZISOFS_TOO_LARGE;
    }

    ret = ziso_create_context(&f, 0);
    if (ret < 0) {
        return ret;
    }
    ret = iso_file_add_filter(file, f, 0);
    free(f);
    if (ret < 0) {
        return ret;
    }
    /* Run a full filter process getsize so that the size is cached */
    stream = iso_file_get_stream(file);
    filtered_size = iso_stream_get_size(stream);
    if (filtered_size < 0) {
        iso_file_remove_filter(file, 0);
        return filtered_size;
    }
    if ((filtered_size >= original_size && !(flag & 1)) ||
        filtered_size / 2048 >= original_size / 2048){
        ret = iso_file_remove_filter(file, 0);
        if (ret < 0) {
            return ret;
        }
        return 2;
    }
    return ISO_SUCCESS;

#else

    return ISO_ZLIB_NOT_ENABLED;

#endif /* ! Libisofs_with_zliB */

}


