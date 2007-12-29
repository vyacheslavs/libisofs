/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "filesrc.h"
#include "error.h"
#include "node.h"
#include "util.h"
#include "writer.h"
#include "messages.h"

#include <stdlib.h>
#include <string.h>

int iso_file_src_cmp(const void *n1, const void *n2)
{
    const IsoFileSrc *f1, *f2;
    int res;
    unsigned int fs_id1, fs_id2;
    dev_t dev_id1, dev_id2;
    ino_t ino_id1, ino_id2;

    f1 = (const IsoFileSrc *)n1;
    f2 = (const IsoFileSrc *)n2;

    res = iso_stream_get_id(f1->stream, &fs_id1, &dev_id1, &ino_id1);
    res = iso_stream_get_id(f2->stream, &fs_id2, &dev_id2, &ino_id2);

    //TODO take care about res <= 0

    if (fs_id1 < fs_id2) {
        return -1;
    } else if (fs_id1 > fs_id2) {
        return 1;
    } else {
        /* files belong to the same fs */
        if (dev_id1 > dev_id2) {
            return -1;
        } else if (dev_id1 < dev_id2) {
            return 1;
        } else {
            /* files belong to same device in same fs */
            return (ino_id1 < ino_id2) ? -1 : (ino_id1 > ino_id2) ? 1 : 0;
        }
    }
}

int iso_file_src_create(Ecma119Image *img, IsoFile *file, IsoFileSrc **src)
{
    int res;
    IsoFileSrc *fsrc;
    unsigned int fs_id;
    dev_t dev_id;
    ino_t ino_id;

    if (img == NULL || file == NULL || src == NULL) {
        return ISO_NULL_POINTER;
    }

    res = iso_stream_get_id(file->stream, &fs_id, &dev_id, &ino_id);
    if (res < 0) {
        return res;
    } else if (res == 0) {
        // TODO this corresponds to Stream that cannot provide a valid id
        // Not implemented for now, but that shouldn't be here, the get_id
        // above is not needed at all, the comparison function should take
        // care of it
        return ISO_ERROR;
    } else {
        int ret;

        fsrc = malloc(sizeof(IsoFileSrc));
        if (fsrc == NULL) {
            return ISO_MEM_ERROR;
        }

        /* fill key and other atts */
        fsrc->prev_img = file->msblock ? 1 : 0;
        fsrc->block = file->msblock;
        fsrc->sort_weight = file->sort_weight;
        fsrc->stream = file->stream;

        /* insert the filesrc in the tree */
        ret = iso_rbtree_insert(img->files, fsrc, (void**)src);
        if (ret <= 0) {
            free(fsrc);
            return ret;
        }
    }
    return ISO_SUCCESS;
}

void iso_file_src_free(void *node)
{
    free(node);
}

off_t iso_file_src_get_size(IsoFileSrc *file)
{
    return iso_stream_get_size(file->stream);
}

static int cmp_by_weight(const void *f1, const void *f2)
{
    IsoFileSrc *f = *((IsoFileSrc**)f1);
    IsoFileSrc *g = *((IsoFileSrc**)f2);
    /* higher weighted first */
    return g->sort_weight - f->sort_weight;
}

static
int filesrc_writer_compute_data_blocks(IsoImageWriter *writer)
{
    size_t i, size;
    Ecma119Image *t;
    IsoFileSrc **filelist;

    if (writer == NULL) {
        return ISO_MEM_ERROR;
    }

    t = writer->target;

    /* store the filesrcs in a array */
    filelist = (IsoFileSrc**)iso_rbtree_to_array(t->files);
    if (filelist == NULL) {
        return ISO_MEM_ERROR;
    }

    size = iso_rbtree_get_size(t->files);

    /* sort files by weight, if needed */
    if (t->sort_files) {
        qsort(t->files, size, sizeof(void*), cmp_by_weight);
    }

    /* fill block value */
    for (i = 0; i < size; ++i) {
        IsoFileSrc *file = filelist[i];
        file->block = t->curblock;
        t->curblock += div_up(iso_file_src_get_size(file), BLOCK_SIZE);
    }

    /* the list is only needed by this writer, store locally */
    writer->data = filelist;
    return ISO_SUCCESS;
}

static
int filesrc_writer_write_vol_desc(IsoImageWriter *writer)
{
    /* nothing needed */
    return ISO_SUCCESS;
}

/* open a file, i.e., its Stream */
static inline
int filesrc_open(IsoFileSrc *file)
{
    return iso_stream_open(file->stream);
}

static inline
int filesrc_close(IsoFileSrc *file)
{
    return iso_stream_close(file->stream);
}

/**
 * @return
 *     1 ok, 0 EOF, < 0 error
 */
static
int filesrc_read(IsoFileSrc *file, char *buf, size_t count)
{
    size_t bytes = 0;

    /* loop to ensure the full buffer is filled */
    do {
        ssize_t result;
        result = iso_stream_read(file->stream, buf + bytes, count - bytes);
        if (result < 0) {
            /* fill buffer with 0s and return */
            memset(buf + bytes, 0, count - bytes);
            return result;
        }
        if (result == 0)
            break;
        bytes += result;
    } while (bytes < count);

    if (bytes < count) {
        /* eof */
        memset(buf + bytes, 0, count - bytes);
        return 0;
    } else {
        return 1;
    }
}

static
int filesrc_writer_write_data(IsoImageWriter *writer)
{
    int res;
    size_t i, b, nfiles;
    Ecma119Image *t;
    IsoFileSrc **filelist;
    char buffer[BLOCK_SIZE];

    if (writer == NULL) {
        return ISO_MEM_ERROR;
    }

    t = writer->target;
    filelist = writer->data;

    iso_msg_debug(t->image, "Writing Files...");

    nfiles = iso_rbtree_get_size(t->files);
    for (i = 0; i < nfiles; ++i) {
        IsoFileSrc *file = filelist[i];

        /*
         * TODO WARNING
         * when we allow files greater than 4GB, current div_up implementation
         * can overflow!!
         */
        uint32_t nblocks = div_up(iso_file_src_get_size(file), BLOCK_SIZE);

        res = filesrc_open(file);
        if (res < 0) {
            /* 
             * UPS, very ugly error, the best we can do is just to write
             * 0's to image
             */
            char *name = iso_stream_get_name(file->stream);
            iso_msg_sorry(t->image, LIBISO_FILE_CANT_WRITE, "File \"%s\" can't"
                          " be opened. Filling with 0s.", name);
            free(name);
            memset(buffer, 0, BLOCK_SIZE);
            for (b = 0; b < nblocks; ++b) {
                res = iso_write(t, buffer, BLOCK_SIZE);
                if (res < 0) {
                    /* ko, writer error, we need to go out! */
                    return res;
                }
            }
            continue;
        }

        /* write file contents to image */
        for (b = 0; b < nblocks; ++b) {
            int wres;
            res = filesrc_read(file, buffer, BLOCK_SIZE);
            wres = iso_write(t, buffer, BLOCK_SIZE);
            if (wres < 0) {
                /* ko, writer error, we need to go out! */
                return wres;
            }
        }

        if (b < nblocks) {
            /* premature end of file, due to error or eof */
            char *name = iso_stream_get_name(file->stream);
            if (res < 0) {
                /* error */
                iso_msg_sorry(t->image, LIBISO_FILE_CANT_WRITE,
                              "Read error in file %s.", name);
            } else {
                /* eof */
                iso_msg_sorry(t->image, LIBISO_FILE_CANT_WRITE,
                              "Premature end of file %s.", name);
            }
            free(name);

            /* fill with 0s */
            iso_msg_sorry(t->image, LIBISO_FILE_CANT_WRITE, "Filling with 0");
            memset(buffer, 0, BLOCK_SIZE);
            while (b++ < nblocks) {
                res = iso_write(t, buffer, BLOCK_SIZE);
                if (res < 0) {
                    /* ko, writer error, we need to go out! */
                    return res;
                }
            }
        }

        filesrc_close(file);
    }

    return ISO_SUCCESS;
}

static
int filesrc_writer_free_data(IsoImageWriter *writer)
{
    /* free the list of files (contents are free together with the tree) */
    free(writer->data);
    return ISO_SUCCESS;
}

int iso_file_src_writer_create(Ecma119Image *target)
{
    IsoImageWriter *writer;

    writer = malloc(sizeof(IsoImageWriter));
    if (writer == NULL) {
        return ISO_MEM_ERROR;
    }

    writer->compute_data_blocks = filesrc_writer_compute_data_blocks;
    writer->write_vol_desc = filesrc_writer_write_vol_desc;
    writer->write_data = filesrc_writer_write_data;
    writer->free_data = filesrc_writer_free_data;
    writer->data = NULL;
    writer->target = target;

    /* add this writer to image */
    target->writers[target->nwriters++] = writer;

    return ISO_SUCCESS;
}
