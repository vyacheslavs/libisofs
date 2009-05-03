/*
 * Copyright (c) 2007 Vreixo Formoso
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "filesrc.h"
#include "node.h"
#include "util.h"
#include "writer.h"
#include "messages.h"
#include "image.h"
#include "stream.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ts A90502 : outsourced comparison to iso_stream_cmp_ino()
*/
int iso_file_src_cmp(const void *n1, const void *n2)
{
    int ret;
    const IsoFileSrc *f1, *f2;

    if (n1 == n2) {
        return 0; /* Normally just a shortcut.
                     But important if Libisofs_file_src_cmp_non_zerO */
    }

    f1 = (const IsoFileSrc *)n1;
    f2 = (const IsoFileSrc *)n2;

    ret = iso_stream_cmp_ino(f1->stream, f2->stream, 0);
    return ret;
}

int iso_file_src_create(Ecma119Image *img, IsoFile *file, IsoFileSrc **src)
{
    int ret;
    IsoFileSrc *fsrc;
    unsigned int fs_id;
    dev_t dev_id;
    ino_t ino_id;

    if (img == NULL || file == NULL || src == NULL) {
        return ISO_NULL_POINTER;
    }

    iso_stream_get_id(file->stream, &fs_id, &dev_id, &ino_id);

    fsrc = calloc(1, sizeof(IsoFileSrc));
    if (fsrc == NULL) {
        return ISO_OUT_OF_MEM;
    }

    /* fill key and other atts */
    fsrc->prev_img = file->from_old_session;
    if (file->from_old_session && img->appendable) {
        /*
         * On multisession discs we keep file sections from old image.
         */
        int ret = iso_file_get_old_image_sections(file, &(fsrc->nsections),
                                                  &(fsrc->sections), 0);
        if (ret < 0) {
            free(fsrc);
            return ISO_OUT_OF_MEM;
        }
    } else {

        /*
         * For new files, or for image copy, we compute our own file sections.
         * Block and size of each section will be filled later.
         */
        off_t section_size = iso_stream_get_size(file->stream);
        if (section_size > (off_t) MAX_ISO_FILE_SECTION_SIZE) {
            fsrc->nsections = DIV_UP(section_size - (off_t) MAX_ISO_FILE_SECTION_SIZE,
                                     (off_t)ISO_EXTENT_SIZE) + 1;
        } else {
            fsrc->nsections = 1;
        }
        fsrc->sections = calloc(fsrc->nsections, sizeof(struct iso_file_section));
    }
    fsrc->sort_weight = file->sort_weight;
    fsrc->stream = file->stream;

    /* insert the filesrc in the tree */
    ret = iso_rbtree_insert(img->files, fsrc, (void**)src);
    if (ret <= 0) {
        free(fsrc->sections);
        free(fsrc);
        return ret;
    }
    iso_stream_ref(fsrc->stream);
    return ISO_SUCCESS;
}

/**
 * Add a given IsoFileSrc to the given image target.
 *
 * The IsoFileSrc will be cached in a tree to prevent the same file for
 * being written several times to image. If you call again this function
 * with a node that refers to the same source file, the previously
 * created one will be returned.
 *
 * @param img
 *      The image where this file is to be written
 * @param new
 *      The IsoFileSrc to add
 * @param src
 *      Will be filled with a pointer to the IsoFileSrc really present in
 *      the tree. It could be different than new if the same file already
 *      exists in the tree.
 * @return
 *      1 on success, 0 if file already exists on tree, < 0 error
 */
int iso_file_src_add(Ecma119Image *img, IsoFileSrc *new, IsoFileSrc **src)
{
    int ret;

    if (img == NULL || new == NULL || src == NULL) {
        return ISO_NULL_POINTER;
    }

    /* insert the filesrc in the tree */
    ret = iso_rbtree_insert(img->files, new, (void**)src);
    return ret;
}

void iso_file_src_free(void *node)
{
    iso_stream_unref(((IsoFileSrc*)node)->stream);
    free(((IsoFileSrc*)node)->sections);
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
int is_ms_file(void *arg)
{
    IsoFileSrc *f = (IsoFileSrc *)arg;
    return f->prev_img ? 0 : 1;
}

static
int filesrc_writer_compute_data_blocks(IsoImageWriter *writer)
{
    size_t i, size;
    Ecma119Image *t;
    IsoFileSrc **filelist;
    int (*inc_item)(void *);

    if (writer == NULL) {
        return ISO_ASSERT_FAILURE;
    }

    t = writer->target;

    /* on appendable images, ms files shouldn't be included */
    if (t->appendable) {
        inc_item = is_ms_file;
    } else {
        inc_item = NULL;
    }

    /* store the filesrcs in a array */
    filelist = (IsoFileSrc**)iso_rbtree_to_array(t->files, inc_item, &size);
    if (filelist == NULL) {
        return ISO_OUT_OF_MEM;
    }

    /* sort files by weight, if needed */
    if (t->sort_files) {
        qsort(filelist, size, sizeof(void*), cmp_by_weight);
    }

    /* fill block value */
    for (i = 0; i < size; ++i) {
        int extent = 0;
        IsoFileSrc *file = filelist[i];

        off_t section_size = iso_stream_get_size(file->stream);
        for (extent = 0; extent < file->nsections - 1; ++extent) {
            file->sections[extent].block = t->curblock + extent *
                        (ISO_EXTENT_SIZE / BLOCK_SIZE);
            file->sections[extent].size = ISO_EXTENT_SIZE;
            section_size -= (off_t) ISO_EXTENT_SIZE;
        }

        /*
         * final section
         */
        file->sections[extent].block = t->curblock + extent * (ISO_EXTENT_SIZE / BLOCK_SIZE);
        file->sections[extent].size = (uint32_t)section_size;

        t->curblock += DIV_UP(iso_file_src_get_size(file), BLOCK_SIZE);
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
    size_t i, b;
    Ecma119Image *t;
    IsoFileSrc *file;
    IsoFileSrc **filelist;
    char name[PATH_MAX];
    char buffer[BLOCK_SIZE];

    if (writer == NULL) {
        return ISO_ASSERT_FAILURE;
    }

    t = writer->target;
    filelist = writer->data;

    iso_msg_debug(t->image->id, "Writing Files...");

    i = 0;
    while ((file = filelist[i++]) != NULL) {

        uint32_t nblocks = DIV_UP(iso_file_src_get_size(file), BLOCK_SIZE);

        res = filesrc_open(file);
        iso_stream_get_file_name(file->stream, name);
        if (res < 0) {
            /*
             * UPS, very ugly error, the best we can do is just to write
             * 0's to image
             */
            iso_report_errfile(name, ISO_FILE_CANT_WRITE, 0, 0);
            res = iso_msg_submit(t->image->id, ISO_FILE_CANT_WRITE, res,
                      "File \"%s\" can't be opened. Filling with 0s.", name);
            if (res < 0) {
                return res; /* aborted due to error severity */
            }
            memset(buffer, 0, BLOCK_SIZE);
            for (b = 0; b < nblocks; ++b) {
                res = iso_write(t, buffer, BLOCK_SIZE);
                if (res < 0) {
                    /* ko, writer error, we need to go out! */
                    return res;
                }
            }
            continue;
        } else if (res > 1) {
            iso_report_errfile(name, ISO_FILE_CANT_WRITE, 0, 0);
            res = iso_msg_submit(t->image->id, ISO_FILE_CANT_WRITE, 0,
                      "Size of file \"%s\" has changed. It will be %s", name,
                      (res == 2 ? "truncated" : "padded with 0's"));
            if (res < 0) {
                filesrc_close(file);
                return res; /* aborted due to error severity */
            }
        }
#ifdef LIBISOFS_VERBOSE_DEBUG
        else {
            iso_msg_debug(t->image->id, "Writing file %s", name);
        }
#endif

        /* write file contents to image */
        for (b = 0; b < nblocks; ++b) {
            int wres;
            res = filesrc_read(file, buffer, BLOCK_SIZE);
            if (res < 0) {
                /* read error */
                break;
            }
            wres = iso_write(t, buffer, BLOCK_SIZE);
            if (wres < 0) {
                /* ko, writer error, we need to go out! */
                filesrc_close(file);
                return wres;
            }
        }

        filesrc_close(file);

        if (b < nblocks) {
            /* premature end of file, due to error or eof */
            iso_report_errfile(name, ISO_FILE_CANT_WRITE, 0, 0);
            if (res < 0) {
                /* error */
                res = iso_msg_submit(t->image->id, ISO_FILE_CANT_WRITE, res,
                               "Read error in file %s.", name);
            } else {
                /* eof */
                res = iso_msg_submit(t->image->id, ISO_FILE_CANT_WRITE, 0,
                              "Premature end of file %s.", name);
            }

            if (res < 0) {
                return res; /* aborted due error severity */
            }

            /* fill with 0s */
            iso_msg_submit(t->image->id, ISO_FILE_CANT_WRITE, 0,
                           "Filling with 0");
            memset(buffer, 0, BLOCK_SIZE);
            while (b++ < nblocks) {
                res = iso_write(t, buffer, BLOCK_SIZE);
                if (res < 0) {
                    /* ko, writer error, we need to go out! */
                    return res;
                }
            }
        }
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
        return ISO_ASSERT_FAILURE;
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
