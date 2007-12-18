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
#include "util.h"

#include "libburn/libburn.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

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
    free(t->input_charset);
    free(t->writers);
    free(t);
}

/**
 * Compute the size of a directory entry for a single node
 */
static
size_t calc_dirent_len(Ecma119Node *n)
{
    int ret = n->iso_name ? strlen(n->iso_name) + 33 : 34;
    if (ret % 2) ret++;
    return ret;
}

/**
 * Computes the total size of all directory entries of a single dir,
 * acording to ECMA-119 6.8.1.1
 */
static 
size_t calc_dir_size(Ecma119Image *t, Ecma119Node *dir)
{
    size_t i, len;

    t->ndirs++;
    
    /* size of "." and ".." entries */
    len = 34 + 34;
    for (i = 0; i < dir->info.dir.nchildren; ++i) {
        Ecma119Node *child = dir->info.dir.children[i];
        size_t dirent_len = calc_dirent_len(child);
        size_t remaining = BLOCK_SIZE - (len % BLOCK_SIZE);
        if (dirent_len > remaining) {
            /* child directory entry doesn't fit on block */
            len += remaining + dirent_len;
        } else {
            len += dirent_len;
        }
    }
    return len;
}

static 
void calc_dir_pos(Ecma119Image *t, Ecma119Node *dir)
{
    size_t i, len;

    dir->info.dir.block = t->curblock;
    len = calc_dir_size(t, dir);
    t->curblock += div_up(len, BLOCK_SIZE);
    for (i = 0; i < dir->info.dir.nchildren; i++) {
        Ecma119Node *child = dir->info.dir.children[i];
        if (child->type == ECMA119_DIR) {
            calc_dir_pos(t, child);
        }
    }
}

static
int ecma119_writer_compute_data_blocks(IsoImageWriter *writer)
{
    Ecma119Image *target;
    Ecma119Node **pathlist;
    uint32_t path_table_size;
    size_t i, j, cur;
    
    if (writer == NULL) {
        return ISO_MEM_ERROR;
    }
    
    target = writer->target;
    
    /* compute position of directories */
    target->ndirs = 0;
    calc_dir_pos(target, target->root);
    
    /* compute length of pathlist */
    pathlist = calloc(1, sizeof(void*) * target->ndirs);
    if (pathlist == NULL) {
        return ISO_MEM_ERROR;
    }
    pathlist[0] = target->root;
    path_table_size = 10; /* root directory record */
    cur = 1;
    for (i = 0; i < target->ndirs; i++) {
        Ecma119Node *dir = pathlist[i];
        for (j = 0; j < dir->info.dir.nchildren; j++) {
            Ecma119Node *child = dir->info.dir.children[j];
            if (child->type == ECMA119_DIR) {
                size_t len = 8 + strlen(child->iso_name);
                pathlist[cur++] = child;
                path_table_size += len + len % 2;
            }
        }
    }
    
    /* compute location for path tables */
    target->l_path_table_pos = target->curblock;
    target->curblock += div_up(path_table_size, BLOCK_SIZE);
    target->m_path_table_pos = target->curblock;
    target->curblock += div_up(path_table_size, BLOCK_SIZE);
    target->path_table_size = path_table_size;
    
    /* ...and free path table cache, as we do not need it at all */
    free(pathlist);
    
    return ISO_SUCCESS;
}

/**
 * Write the Primary Volume Descriptor
 */
static
int ecma119_writer_write_vol_desc(IsoImageWriter *writer)
{
    IsoImage *image;
    Ecma119Image *t;
    struct ecma119_pri_vol_desc vol;
    
    char *vol_id, *pub_id, *data_id, *volset_id;
    char *system_id, *application_id, *copyright_file_id;
    char *abstract_file_id, *biblio_file_id;
    
    if (writer == NULL) {
        return ISO_MEM_ERROR;
    }
    
    t = writer->target;
    image = t->image;
    
    memset(&vol, 0, sizeof(struct ecma119_pri_vol_desc));
    
    str2d_char(image->volume_id, t->input_charset, &vol_id);
    str2a_char(image->publisher_id, t->input_charset, &pub_id);
    str2a_char(image->data_preparer_id, t->input_charset, &data_id);
    str2d_char(image->volset_id, t->input_charset, &volset_id);
    
    str2a_char(image->system_id, t->input_charset, &system_id);
    str2a_char(image->application_id, t->input_charset, &application_id);
    str2d_char(image->copyright_file_id, t->input_charset, &copyright_file_id);
    str2d_char(image->abstract_file_id, t->input_charset, &abstract_file_id);
    str2d_char(image->biblio_file_id, t->input_charset, &biblio_file_id);
    
    vol.vol_desc_type[0] = 1;
    memcpy(vol.std_identifier, "CD001", 5);
    vol.vol_desc_version[0] = 1;
    if (system_id)
        strncpy((char*)vol.system_id, system_id, 32);
    else
        /* put linux by default? */
        memcpy(vol.system_id, "LINUX", 5); 
    if (vol_id)
        strncpy((char*)vol.volume_id, vol_id, 32);
    iso_bb(vol.vol_space_size, t->vol_space_size, 4);
    iso_bb(vol.vol_set_size, 1, 2);
    iso_bb(vol.vol_seq_number, 1, 2);
    iso_bb(vol.block_size, BLOCK_SIZE, 2);
    iso_bb(vol.path_table_size, t->path_table_size, 4);
    iso_lsb(vol.l_path_table_pos, t->l_path_table_pos, 4);
    iso_msb(vol.m_path_table_pos, t->m_path_table_pos, 4);

    //TODO
    //write_one_dir_record(t, t->root, 3, vol->root_dir_record);

    if (volset_id)
        strncpy((char*)vol.vol_set_id, volset_id, 128);
    if (pub_id)
        strncpy((char*)vol.publisher_id, pub_id, 128);
    if (data_id)
        strncpy((char*)vol.data_prep_id, data_id, 128);
    
    if (application_id)
        strncpy((char*)vol.application_id, application_id, 128);
    if (copyright_file_id)
        strncpy((char*)vol.copyright_file_id, copyright_file_id, 37);
    if (abstract_file_id)
        strncpy((char*)vol.abstract_file_id, abstract_file_id, 37);
    if (biblio_file_id)
        strncpy((char*)vol.bibliographic_file_id, biblio_file_id, 37);

    iso_datetime_17(vol.vol_creation_time, t->now);
    iso_datetime_17(vol.vol_modification_time, t->now);
    iso_datetime_17(vol.vol_effective_time, t->now);
    vol.file_structure_version[0] = 1;

    free(vol_id);
    free(volset_id);
    free(pub_id);
    free(data_id);
    free(system_id);
    free(application_id);
    free(copyright_file_id);
    free(abstract_file_id);
    free(biblio_file_id);
    
    /* Finally write the Volume Descriptor */
    return iso_write(t, &vol, sizeof(struct ecma119_pri_vol_desc));
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
    target->sort_files = opts->sort_files;
    
    target->now = time(NULL);
    target->ms_block = 0;
    target->input_charset = strdup("UTF-8"); //TODO
    
    /* 
     * 2. Based on those options, create needed writers: iso, joliet...
     * Each writer inits its structures and stores needed info into
     * target. 
     * If the writer needs an volume descriptor, it increments image
     * current block.
     * Finally, create Writer for files.
     */
    target->curblock = target->ms_block + 16; 
    
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
    
    /*
     * The volume space size is just the size of the last session, in
     * case of ms images.
     */
    target->total_size = (target->curblock - target->ms_block) * BLOCK_SIZE;
    target->vol_space_size = target->curblock - target->ms_block;
    
    
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

int iso_write(Ecma119Image *target, void *buf, size_t count)
{
    ssize_t result;
    
    result = write(target->wrfd, buf, count);
    if (result != count) {
        /* treat this as an error? */
        return ISO_WRITE_ERROR;
    } else {
        return ISO_SUCCESS;
    }
}
