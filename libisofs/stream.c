/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2009 Thomas Schmitt
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 */

#include "libisofs.h"
#include "stream.h"
#include "fsource.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>


#ifndef PATH_MAX
#define PATH_MAX Libisofs_default_path_maX
#endif


ino_t serial_id = (ino_t)1;
ino_t mem_serial_id = (ino_t)1;
ino_t cut_out_serial_id = (ino_t)1;

static
int fsrc_open(IsoStream *stream)
{
    int ret;
    struct stat info;
    off_t esize;
    IsoFileSource *src;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = ((FSrcStreamData*)stream->data)->src;
    ret = iso_file_source_stat(src, &info);
    if (ret < 0) {
        return ret;
    }
    ret = iso_file_source_open(src);
    if (ret < 0) {
        return ret;
    }
    esize = ((FSrcStreamData*)stream->data)->size;
    if (info.st_size == esize) {
        return ISO_SUCCESS;
    } else {
        return (esize > info.st_size) ? 3 : 2;
    }
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
void fsrc_free(IsoStream *stream)
{
    FSrcStreamData *data;
    data = (FSrcStreamData*)stream->data;
    iso_file_source_unref(data->src);
    free(data);
}

static
int fsrc_update_size(IsoStream *stream)
{
    int ret;
    struct stat info;
    IsoFileSource *src;

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = ((FSrcStreamData*)stream->data)->src;
    ret = iso_file_source_stat(src, &info);
    if (ret < 0) {
        return ret;
    }

    ((FSrcStreamData*)stream->data)->size = info.st_size;
    return ISO_SUCCESS;
}

IsoStreamIface fsrc_stream_class = {
    1, /* update_size is defined for this stream */
    "fsrc",
    fsrc_open,
    fsrc_close,
    fsrc_get_size,
    fsrc_read,
    fsrc_is_repeatable,
    fsrc_get_id,
    fsrc_free,
    fsrc_update_size
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
    if (data == NULL) {
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


int iso_stream_get_src_zf(IsoStream *stream, int *header_size_div4,
                          int *block_size_log2, uint32_t *uncompressed_size,
                          int flag)
{
    int ret;
    FSrcStreamData *data;
    IsoFileSource *src;

    /* Intimate friendship with libisofs/fs_image.c */
    int iso_ifs_source_get_zf(IsoFileSource *src, int *header_size_div4,
                  int *block_size_log2, uint32_t *uncompressed_size, int flag);

    if (stream->class != &fsrc_stream_class)
        return 0;
    data = stream->data;
    src = data->src;

    ret = iso_ifs_source_get_zf(src, header_size_div4, block_size_log2,
                                uncompressed_size, 0);
    return ret;
}


struct cut_out_stream
{
    IsoFileSource *src;

    /* key for file identification inside filesystem */
    dev_t dev_id;
    ino_t ino_id;
    off_t offset; /**< offset where read begins */
    off_t size; /**< size of this file */
    off_t pos; /* position on the file for read */
};

static
int cut_out_open(IsoStream *stream)
{
    int ret;
    struct stat info;
    IsoFileSource *src;
    struct cut_out_stream *data;

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }

    data = stream->data;
    src = data->src;
    ret = iso_file_source_stat(data->src, &info);
    if (ret < 0) {
        return ret;
    }
    ret = iso_file_source_open(src);
    if (ret < 0) {
        return ret;
    }

    {
        off_t ret;
        if (data->offset > info.st_size) {
            /* file is smaller than expected */
            ret = iso_file_source_lseek(src, info.st_size, 0);
        } else {
            ret = iso_file_source_lseek(src, data->offset, 0);
        }
        if (ret < 0) {
            return (int) ret;
        }
    }
    data->pos = 0;
    if (data->offset + data->size > info.st_size) {
        return 3; /* file smaller than expected */
    } else {
        return ISO_SUCCESS;
    }
}

static
int cut_out_close(IsoStream *stream)
{
    IsoFileSource *src;
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    src = ((struct cut_out_stream*)stream->data)->src;
    return iso_file_source_close(src);
}

static
off_t cut_out_get_size(IsoStream *stream)
{
    struct cut_out_stream *data = stream->data;
    return data->size;
}

static
int cut_out_read(IsoStream *stream, void *buf, size_t count)
{
    struct cut_out_stream *data = stream->data;
    count = (size_t)MIN(data->size - data->pos, count);
    if (count == 0) {
        return 0;
    }
    return iso_file_source_read(data->src, buf, count);
}

static
int cut_out_is_repeatable(IsoStream *stream)
{
    /* reg files are always repeatable */
    return 1;
}

static
void cut_out_get_id(IsoStream *stream, unsigned int *fs_id, dev_t *dev_id,
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
void cut_out_free(IsoStream *stream)
{
    struct cut_out_stream *data = stream->data;
    iso_file_source_unref(data->src);
    free(data);
}

/*
 * TODO update cut out streams to deal with update_size(). Seems hard.
 */
IsoStreamIface cut_out_stream_class = {
    0,
    "cout",
    cut_out_open,
    cut_out_close,
    cut_out_get_size,
    cut_out_read,
    cut_out_is_repeatable,
    cut_out_get_id,
    cut_out_free
};

int iso_cut_out_stream_new(IsoFileSource *src, off_t offset, off_t size,
                           IsoStream **stream)
{
    int r;
    struct stat info;
    IsoStream *str;
    struct cut_out_stream *data;

    if (src == NULL || stream == NULL) {
        return ISO_NULL_POINTER;
    }
    if (size == 0) {
        return ISO_WRONG_ARG_VALUE;
    }

    r = iso_file_source_stat(src, &info);
    if (r < 0) {
        return r;
    }
    if (!S_ISREG(info.st_mode)) {
        return ISO_WRONG_ARG_VALUE;
    }
    if (offset > info.st_size) {
        return ISO_FILE_OFFSET_TOO_BIG;
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
    data = malloc(sizeof(struct cut_out_stream));
    if (data == NULL) {
        free(str);
        return ISO_OUT_OF_MEM;
    }

    /* take a new ref to IsoFileSource */
    data->src = src;
    iso_file_source_ref(src);

    data->offset = offset;
    data->size = MIN(info.st_size - offset, size);

    /* get the id numbers */
    data->dev_id = (dev_t) 0;
    data->ino_id = cut_out_serial_id++;

    str->refcount = 1;
    str->data = data;
    str->class = &cut_out_stream_class;

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
        return ISO_FILE_ALREADY_OPENED;
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
        return ISO_FILE_NOT_OPENED;
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
        return ISO_FILE_NOT_OPENED;
    }

    if (data->offset >= data->size) {
        return 0; /* EOF */
    }

    len = MIN(count, data->size - data->offset);
    memcpy(buf, data->buf + data->offset, len);
    data->offset += len;
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
void mem_free(IsoStream *stream)
{
    MemStreamData *data;
    data = (MemStreamData*)stream->data;
    free(data->buf);
    free(data);
}

IsoStreamIface mem_stream_class = {
    0,
    "mem ",
    mem_open,
    mem_close,
    mem_get_size,
    mem_read,
    mem_is_repeatable,
    mem_get_id,
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
int iso_stream_update_size(IsoStream *stream)
{
    IsoStreamIface* class = stream->class;
    return (class->version >= 1) ? class->update_size(stream) : 0;
}

inline
void iso_stream_get_id(IsoStream *stream, unsigned int *fs_id, dev_t *dev_id,
                      ino_t *ino_id)
{
    stream->class->get_id(stream, fs_id, dev_id, ino_id);
}

void iso_stream_get_file_name(IsoStream *stream, char *name)
{
    char *type = stream->class->type;

    if (!strncmp(type, "fsrc", 4)) {
        FSrcStreamData *data = stream->data;
        char *path = iso_file_source_get_path(data->src);
        strncpy(name, path, PATH_MAX);
        free(path);
    } else if (!strncmp(type, "boot", 4)) {
        strcpy(name, "BOOT CATALOG");
    } else if (!strncmp(type, "mem ", 4)) {
        strcpy(name, "MEM SOURCE");
    } else if (!strncmp(type, "extf", 4)) {
        strcpy(name, "EXTERNAL FILTER");
    } else {
        strcpy(name, "UNKNOWN SOURCE");
    }
}

IsoStream *iso_stream_get_input_stream(IsoStream *stream, int flag)
{
    IsoStreamIface* class;

    if (stream == NULL) {
        return NULL;
    }
    class = stream->class;
    if (class->version < 2)
        return NULL;
    return class->get_input_stream(stream, 0);
}

char *iso_stream_get_source_path(IsoStream *stream, int flag)
{
    char *path = NULL, ivd[80], *raw_path = NULL;

    if (stream == NULL) {
        return NULL;
    }
    if (stream->class == &fsrc_stream_class) {
        FSrcStreamData *fsrc_data = stream->data;

        path = iso_file_source_get_path(fsrc_data->src);
    } else if (stream->class == &cut_out_stream_class) {
        struct cut_out_stream *cout_data = stream->data;

        raw_path = iso_file_source_get_path(cout_data->src);
        sprintf(ivd, " %.f %.f",
                (double) cout_data->offset, (double) cout_data->size);
        path= calloc(strlen(raw_path) + strlen(ivd) + 1, 1);
        if (path == NULL) {
            goto ex;
        }
        strcpy(path, raw_path);
        strcat(path, ivd);
    }
ex:;
    if (raw_path != NULL)
        free(raw_path);
    return path;
}

/* @return 1 = ok , 0 = not an ISO image stream , <0 = error */
int iso_stream_set_image_ino(IsoStream *stream, ino_t ino, int flag)
{
    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    if (stream->class == &fsrc_stream_class) {
        FSrcStreamData *fsrc_data = stream->data;
        fsrc_data->ino_id = ino;
        return 1;
    }
   return 0;
}

/* API */ 
int iso_stream_cmp_ino(IsoStream *s1, IsoStream *s2, int flag)
{
    int ret;
    unsigned int fs_id1, fs_id2;
    dev_t dev_id1, dev_id2;
    ino_t ino_id1, ino_id2;
    off_t size1, size2;
    FSrcStreamData *fssd1, *fssd2;


/*
   #define Libisofs_stream_cmp_ino_debuG 1
*/
#ifdef Libisofs_stream_cmp_ino_debuG
    static int report_counter = 0;
    static int debug = 1;
#endif /* Libisofs_stream_cmp_ino_debuG */

    if (s1 == s2)
        return 0;
    if (s1 == NULL)
        return -1;
    if (s2 == NULL)
        return 1;

    if (s1->class->version >= 3 && !(flag & 1)) {
       /* Filters may have smarter methods to compare themselves with others */
       ret = s1->class->cmp_ino(s1, s2);
       return ret;
    }

    iso_stream_get_id(s1, &fs_id1, &dev_id1, &ino_id1);
    iso_stream_get_id(s2, &fs_id2, &dev_id2, &ino_id2);
    if (fs_id1 < fs_id2) {
        return -1;
    } else if (fs_id1 > fs_id2) {
        return 1;
    }
    /* files belong to the same fs */
    if (dev_id1 > dev_id2) {
        return -1;
    } else if (dev_id1 < dev_id2) {
        return 1;
    } else if (ino_id1 < ino_id2) {
        return -1;
    } else if (ino_id1 > ino_id2) {
        return 1;
    }
    size1 = iso_stream_get_size(s1);
    size2 = iso_stream_get_size(s2);
    if (size1 < size2) {

#ifdef Libisofs_stream_cmp_ino_debuG
        if (debug) {
            if (report_counter < 5)
                fprintf(stderr,
      "\n\nlibisofs_DEBUG : Program error: same ino but differing size\n\n\n");
            else if (report_counter == 5)
                fprintf(stderr,
      "\n\nlibisofs_DEBUG : Inode error: more of same ino but differing size\n\n\n");
            report_counter++;
        }
#endif /* Libisofs_stream_cmp_ino_debuG */

        return -1;
    } else if (size1 > size2) {

#ifdef Libisofs_stream_cmp_ino_debuG
        if (debug) {
            if (report_counter < 5)
                fprintf(stderr,
      "\n\nlibisofs_DEBUG : Inode error: same ino but differing size\n\n\n");
            else if (report_counter == 5)
                fprintf(stderr,
      "\n\nlibisofs_DEBUG : Program error: more of same ino but differing size\n\n\n");
            report_counter++;
        }
#endif /* Libisofs_stream_cmp_ino_debuG */

        return 1;
    }

    if (s1->class != s2->class)
        return (s1->class < s2->class ? -1 : 1);
    if (s1->class == &fsrc_stream_class) {
        /* Compare eventual image data section LBA and sizes */
        fssd1= (FSrcStreamData *) s1->data;
        fssd2= (FSrcStreamData *) s2->data;
        ret = iso_ifs_sections_cmp(fssd1->src, fssd2->src, 0);
        if (ret != 0)
            return ret;
    }
    if (fs_id1 == 0 && dev_id1 == 0 && ino_id1 == 0) {
        return (s1 < s2 ? -1 : 1);
    }
    return 0;
}


/**
 * @return
 *     1 ok, 0 EOF, < 0 error
 */    
int iso_stream_read_buffer(IsoStream *stream, char *buf, size_t count,
                           size_t *got)
{
    ssize_t result;

    *got = 0;
    do {
        result = iso_stream_read(stream, buf + *got, count - *got);
        if (result < 0) {
            memset(buf + *got, 0, count - *got);
            return result;
        }
        if (result == 0)
            break;
        *got += result;
    } while (*got < count);

    if (*got < count) {
        /* eof */
        memset(buf + *got, 0, count - *got);
        return 0;
    }
    return 1;
}

#ifdef Libisofs_with_checksumS


/* @param flag bit0= dig out most original stream (e.g. because from old image)
   @return 1=ok, md5 is valid,
           0= not ok, 
          <0 fatal error, abort 
*/  
int iso_stream_make_md5(IsoStream *stream, char md5[16], int flag)
{
    int res, is_open = 0;
    char buffer[2048];
    void *ctx= NULL;
    off_t file_size;
    uint32_t b, nblocks;
    size_t got_bytes;
    IsoStream *input_stream;

    if (flag & 1) {
        while(1) {
           input_stream = iso_stream_get_input_stream(stream, 0);
           if (input_stream == NULL)
        break;
           stream = input_stream;
        }
    }

    if (! iso_stream_is_repeatable(stream))
        return 0;
    res = iso_md5_start(&ctx);
    if (res < 0)
        return res;
    res = iso_stream_open(stream);
    if (res < 0)
        return 0;
    is_open = 1;
    file_size = iso_stream_get_size(stream);
    nblocks = DIV_UP(file_size, 2048);
    for (b = 0; b < nblocks; ++b) {
        res = iso_stream_read_buffer(stream, buffer, 2048, &got_bytes);
        if (res < 0) {
            res = 0;
            goto ex;
        }
        /* Do not use got_bytes to stay closer to IsoFileSrc processing */
        if (file_size - b * 2048 > 2048)
            res = 2048;
        else
            res = file_size - b * 2048;
        iso_md5_compute(ctx, buffer, res);
    }
    res = 1;
ex:;
    if (is_open)
        iso_stream_close(stream);
    if (ctx != NULL)
        iso_md5_end(&ctx, md5);
    return res;
}

#endif /* Libisofs_with_checksumS */

