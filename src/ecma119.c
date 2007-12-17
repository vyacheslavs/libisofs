/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "libisofs.h"
#include "ecma119.h"
#include "ecma119_tree.h"
#include "error.h"
#include "filesrc.h"
#include "image.h"
#include "writer.h"

#include "libburn/libburn.h"

#include <stdlib.h>
#include <time.h>

static
void ecma119_image_free(Ecma119Image *t)
{
    size_t i;
    
    ecma119_node_free(t->root);
    iso_image_unref(t->image);
    iso_file_src_free(t);
    
    for (i = 0; i < t->nwriters; ++i) {
        IsoImageWriter *writer = t->writers[i];
        writer->free_data(writer);
        free(writer);
    }
    free(t->writers);
    free(t);
}

static
int ecma119_writer_compute_data_blocks(IsoImageWriter *writer)
{
    //TODO to implement
    return -1;
}

static
int ecma119_writer_write_vol_desc(IsoImageWriter *writer)
{
    //TODO to implement
    return -1;
}

static
int ecma119_writer_write_data(IsoImageWriter *writer)
{
    //TODO to implement
    return -1;
}

static
int ecma119_writer_free_data(IsoImageWriter *writer)
{
    //TODO to implement
    return -1;
}

int ecma119_writer_create(Ecma119Image *target)
{
    int ret;
    IsoImageWriter *writer;
    
    writer = malloc(sizeof(IsoImageWriter));
    if (writer == NULL) {
        return ISO_MEM_ERROR;
    }
    
    writer->compute_data_blocks = ecma119_writer_compute_data_blocks;
    writer->write_vol_desc = ecma119_writer_write_vol_desc;
    writer->write_data = ecma119_writer_write_data;
    writer->free_data = ecma119_writer_free_data;
    writer->data = NULL;
    writer->target = target;
    
    /* add this writer to image */
    target->writers[target->nwriters++] = writer;
    
    ret = ecma119_tree_create(target);
    if (ret < 0) {
        return ret;
    }
    
    /* we need the volume descriptor */
    target->curblock++;
    return ISO_SUCCESS;
}

static 
int ecma119_image_new(IsoImage *src, Ecma119WriteOpts *opts, 
                      Ecma119Image **img)
{
    int ret, i;
    Ecma119Image *target;
    
    /* 1. Allocate target and copy opts there */
    target = calloc(1, sizeof(Ecma119Image));
    if (target == NULL) {
        return ISO_MEM_ERROR;
    }
    
    target->image = src;
    iso_image_ref(src);
    
    target->iso_level = opts->level;
    
    target->now = time(NULL);
    
    /* 
     * 2. Based on those options, create needed writers: iso, joliet...
     * Each writer inits its structures and stores needed info into
     * target. 
     * If the writer needs an volume descriptor, it increments image
     * current block.
     * Finally, create Writer for files.
     */
    target->curblock = 16; 
    
    /* the number of writers is dependent of the extensions */
    target->writers = malloc(2 * sizeof(void*));
    if (target->writers == NULL) {
        iso_image_unref(src);
        free(target);
        return ISO_MEM_ERROR;
    }
    
    /* create writer for ECMA-119 structure */
    ret = ecma119_writer_create(target);
    if (ret < 0) {
        goto target_cleanup;
    }
    
    /* Volume Descriptor Set Terminator */
    target->curblock++;
    
    
    /*
     * 3. 
     * Call compute_data_blocks() in each Writer.
     * That function computes the size needed by its structures and
     * increments image current block propertly.
     */
    for (i = 0; i < target->nwriters; ++i) {
        IsoImageWriter *writer = target->writers[i];
        ret = writer->compute_data_blocks(writer);
        if (ret < 0) {
            goto target_cleanup;
        }
    }
    
    /* 4. Start writting thread */ 
    
    
    
    *img = target;
    return ISO_SUCCESS;
    
target_cleanup:;
    ecma119_image_free(target);
    return ret;
}

static int
bs_read(struct burn_source *bs, unsigned char *buf, int size)
{
    // TODO to implement
    return 0;
}

static off_t
bs_get_size(struct burn_source *bs)
{
    Ecma119Image *image = (Ecma119Image*)bs->data;
    return image->total_size;
}

static void
bs_free_data(struct burn_source *bs)
{
    ecma119_image_free((Ecma119Image*)bs->data);
}

static
int bs_set_size(struct burn_source *bs, off_t size)
{
    //TODO to implement!!
//    struct ecma119_write_target *t = (struct ecma119_write_target*)bs->data;
//    t->total_size = size;
    return 1;
}

int iso_image_create(IsoImage *image, Ecma119WriteOpts *opts,
                     struct burn_source **burn_src)
{
    int ret;
    struct burn_source *source;
    Ecma119Image *target = NULL;
    
    if (image == NULL || opts == NULL || burn_src == NULL) {
        return ISO_NULL_POINTER;
    }
    
    source = calloc(1, sizeof(struct burn_source));
    if (source == NULL) {
        return ISO_MEM_ERROR;
    }
    
    ret = ecma119_image_new(image, opts, &target);
    if (ret < 0) {
        free(source);
        return ret;
    }
    
    source->refcount = 1;
    source->read = bs_read;
    source->get_size = bs_get_size;
    source->set_size = bs_set_size;
    source->free_data = bs_free_data;
    source->data = target;
    return ISO_SUCCESS;
}


