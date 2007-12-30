/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

/*
 * Filesystem/FileSource implementation to access an ISO image, using an
 * IsoDataSource to read image data.
 */

#include "fs_image.h"
#include "error.h"
#include "ecma119.h"

#include <stdlib.h>
#include <string.h>

/**
 * Should the RR extensions be read?
 */
enum read_rr_ext {
    RR_EXT_NO = 0, /*< Do not use RR extensions */
    RR_EXT_110 = 1, /*< RR extensions conforming version 1.10 */
    RR_EXT_112 = 2 /*< RR extensions conforming version 1.12 */
};

/**
 * Private data for the image IsoFilesystem
 */
typedef struct
{
    /** DataSource from where data will be read */
    IsoDataSource *src;
    
    /** 
     * Counter of the times the filesystem has been openned still pending of
     * close. It is used to keep track of when we need to actually open or 
     * close the IsoDataSource. 
     */
    unsigned int open_count;
    
    uid_t uid; /**< Default uid when no RR */
    gid_t gid; /**< Default uid when no RR */
    mode_t mode; /**< Default mode when no RR (only permissions) */

    struct libiso_msgs *messenger;

    char *input_charset; /**< Input charset for RR names */
    
    /**
     * Will be filled with the block lba of the extend for the root directory,
     * as read from the PVM
     */
    uint32_t iso_root_block; 

    /**
     * If we need to read RR extensions. i.e., if the image contains RR 
     * extensions, and the user wants to read them. 
     */
    enum read_rr_ext rr; 
    
    /**
     * The function used to read the name from a directoy record. For ISO, 
     * the name is in US-ASCII. For Joliet, in UCS-2BE. Thus, we need 
     * different functions for both.
     */
    char *(*get_name)(const char *, size_t);
    
    /** 
     * Joliet and RR version 1.10 does not have file serial numbers,
     * we need to generate it. TODO what is this for?!?!?!
     */
    //ino_t ino;
    
    /**
     * Bytes skipped within the System Use field of a directory record, before 
     * the beginning of the SUSP system user entries. See IEEE 1281, SUSP. 5.3. 
     */
    uint8_t len_skp; 

    /* Volume attributes */
    char *volset_id;
    char *volume_id; /**< Volume identifier. */
    char *publisher_id; /**< Volume publisher. */
    char *data_preparer_id; /**< Volume data preparer. */
    char *system_id; /**< Volume system identifier. */
    char *application_id; /**< Volume application id */
    char *copyright_file_id;
    char *abstract_file_id;
    char *biblio_file_id;

    /* extension information */

    /**
     * RR version being used in image.
     * 0 no RR extension, 1 RRIP 1.10, 2 RRIP 1.12
     */
    unsigned int rr_version : 2;
    
    /**
     * Number of blocks of the volume, as reported in the PVM.
     */
    uint32_t nblocks;
    
    //TODO el-torito information
    
} _ImageFsData;

static
int ifs_fs_open(IsoImageFilesystem *fs)
{
    _ImageFsData *data;
    
    if (fs == NULL || fs->fs.data == NULL) {
        return ISO_NULL_POINTER;
    }
    
    data = (_ImageFsData*)fs->fs.data;
    
    if (data->open_count == 0) {
        /* we need to actually open the data source */
        int res = data->src->open(data->src);
        if (res < 0) {
            return res;
        }
    }
    ++data->open_count;
    return ISO_SUCCESS;
}

static
int ifs_fs_close(IsoImageFilesystem *fs)
{
    _ImageFsData *data;
    
    if (fs == NULL || fs->fs.data == NULL) {
        return ISO_NULL_POINTER;
    }
    
    data = (_ImageFsData*)fs->fs.data;
    
    if (--data->open_count == 0) {
        /* we need to actually close the data source */
        return data->src->close(data->src);
    }
    return ISO_SUCCESS;
}

static
void ifs_fs_free(IsoFilesystem *fs)
{
    IsoImageFilesystem *ifs;
    _ImageFsData *data;
    
    ifs = (IsoImageFilesystem*)fs;
    data = (_ImageFsData*) fs->data;
    
    /* close data source if already openned */
    if (data->open_count > 0) {
        data->src->close(data->src);
    }
    
    /* free our ref to datasource */
    iso_data_source_unref(data->src);
    
    /* free volume atts */
    free(data->volset_id);
    free(data->volume_id);
    free(data->publisher_id);
    free(data->data_preparer_id);
    free(data->system_id);
    free(data->application_id);
    free(data->copyright_file_id);
    free(data->abstract_file_id);
    free(data->biblio_file_id);
    
    free(data->input_charset);
    free(data);
}

static
int read_pvm(_ImageFsData *data, uint32_t block)
{
    int ret;
    struct ecma119_pri_vol_desc *pvm;
    struct ecma119_dir_record *rootdr;
    unsigned char buffer[BLOCK_SIZE];
    
    /* read PVM */
    ret = data->src->read_block(data->src, block, buffer);
    if (ret < 0) {
        return ret;
    }
    
    pvm = (struct ecma119_pri_vol_desc *)buffer;
    
    /* sanity checks */
    if (pvm->vol_desc_type[0] != 1 || pvm->vol_desc_version[0] != 1 
        || strncmp((char*)pvm->std_identifier, "CD001", 5)
        || pvm->file_structure_version[0] != 1 ) {

         return ISO_WRONG_PVD;
    }
    
    /* ok, it is a valid PVD */
    
    /* fill volume attributes  */
    data->volset_id = strcopy((char*)pvm->vol_set_id, 128);
    data->volume_id = strcopy((char*)pvm->volume_id, 32);
    data->publisher_id = strcopy((char*)pvm->publisher_id, 128);
    data->data_preparer_id = strcopy((char*)pvm->data_prep_id, 128);
    data->system_id = strcopy((char*)pvm->system_id, 32);
    data->application_id = strcopy((char*)pvm->application_id, 128);
    data->copyright_file_id = strcopy((char*)pvm->copyright_file_id, 37);
    data->abstract_file_id = strcopy((char*)pvm->abstract_file_id, 37);
    data->biblio_file_id = strcopy((char*)pvm->bibliographic_file_id, 37);
    
    data->nblocks = iso_read_bb(pvm->vol_space_size, 4, NULL);
    
    rootdr = (struct ecma119_dir_record*) pvm->root_dir_record;
    data->iso_root_block = iso_read_bb(rootdr->block, 4, NULL);
    
    /*
     * TODO
     * PVM has other things that could be interesting, but that don't have a 
     * member in IsoImage, such as creation date. In a multisession disc, we 
     * could keep the creation date and update the modification date, for 
     * example.
     */
    
    return ISO_SUCCESS;
}

int iso_image_filesystem_new(IsoDataSource *src, struct iso_read_opts *opts,
                             IsoImageFilesystem **fs)
{
    int ret;
    IsoImageFilesystem *ifs;
    _ImageFsData *data;
    
    if (src == NULL || opts == NULL || fs == NULL) {
        return ISO_NULL_POINTER;
    }
    
    data = calloc(1, sizeof(_ImageFsData));
    if (data == NULL) {
        return ISO_MEM_ERROR;
    }
    
    ifs = calloc(1, sizeof(IsoImageFilesystem));
    if (ifs == NULL) {
        free(data);
        return ISO_MEM_ERROR;
    }
    
    /* get our ref to IsoDataSource */
    data->src = src;
    iso_data_source_ref(src);
    data->open_count = 0; //TODO
    
    /* fill data from opts */
    data->gid = opts->gid;
    data->uid = opts->uid;
    data->mode = opts->mode & ~S_IFMT;
    data->input_charset = strdup("UTF-8"); //TODO strdup(opts->input_charset);
    data->messenger = opts->messenger;
    
    ifs->open = ifs_fs_open;
    ifs->close = ifs_fs_close;
    
    ifs->fs.data = data;
    ifs->fs.free = ifs_fs_free;
    
    /* read Volume Descriptors and ensure it is a valid image */
    
    /* 1. first, open the filesystem */
    ifs_fs_open(ifs);

    /* 2. read primary volume description */
    ret = read_pvm(data, opts->block + 16);
    if (ret < 0) {
        goto fs_cleanup;
    }
    
    /* 3. read next volume descriptors */
    // TODO
    
    /* 4. check if RR extensions are being used */
    //TODO
    //ret = read_root_susp_entries(info, volume->root, data->iso_root_block);
    if (ret < 0) {
        return ret;
    }

    /* select what tree to read */
    //TODO
    
    /* and finally return. Note that we keep the DataSource opened */
    
    *fs = ifs;
    return ISO_SUCCESS;
    
fs_cleanup: ;
    ifs_fs_free((IsoFilesystem*)ifs);
    free(ifs);
    return ret;
}
