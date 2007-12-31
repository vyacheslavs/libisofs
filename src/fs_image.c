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
#include "messages.h"
#include "rockridge.h"

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
    enum read_rr_ext rr_version;
    
    /** If Joliet extensions are available on image */
    unsigned int joliet : 1;

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

/**
 * Read the SUSP system user entries of the "." entry of the root directory, 
 * indentifying when Rock Ridge extensions are being used.
 */
static 
int read_root_susp_entries(_ImageFsData *data, uint32_t block)
{
    int ret;
    unsigned char buffer[2048];
    struct ecma119_dir_record *record;
    struct susp_sys_user_entry *sue;
    SuspIterator *iter;

    ret = data->src->read_block(data->src, block, buffer);
    if (ret < 0) {
        return ret;
    }
    
    /* record will be the "." directory entry for the root record */
    record = (struct ecma119_dir_record *)buffer;
    
    /*
     * TODO 
     * SUSP specification claims that for CD-ROM XA the SP entry
     * is not at position BP 1, but at BP 15. Is that used?
     * In that case, we need to set info->len_skp to 15!!
     */

    iter = susp_iter_new(data->src, record, data->len_skp, data->messenger);
    if (iter == NULL) {
        return ISO_MEM_ERROR;
    }
    
    /* first entry must be an SP system use entry */
    ret = susp_iter_next(iter, &sue);
    if (ret < 0) {
        /* error */
        susp_iter_free(iter);
        return ret;
    } else if (ret == 0 || !SUSP_SIG(sue, 'S', 'P') ) {
        iso_msg_debug(data->messenger, "SUSP/RR is not being used.");
        susp_iter_free(iter);
        return ISO_SUCCESS;
    }
    
    /* it is a SP system use entry */
    if (sue->version[0] != 1 || sue->data.SP.be[0] != 0xBE
        || sue->data.SP.ef[0] != 0xEF) {
    
        iso_msg_sorry(data->messenger, LIBISO_SUSP_WRONG, "SUSP SP system use "
            "entry seems to be wrong. Ignoring Rock Ridge Extensions.");
        susp_iter_free(iter);
        return ISO_SUCCESS;
    }
    
    iso_msg_debug(data->messenger, "SUSP/RR is being used.");
    
    /*
     * The LEN_SKP field, defined in IEEE 1281, SUSP. 5.3, specifies the
     * number of bytes to be skipped within each System Use field.
     * I think this will be always 0, but given that support this standard
     * feature is easy...
     */
    data->len_skp = sue->data.SP.len_skp[0];
    
    /*
     * Ok, now search for ER entry.
     * Just notice that the attributes for root dir are read elsewhere.
     * 
     * TODO if several ER are present, we need to identify the position of
     *      what refers to RR, and then look for corresponding ES entry in
     *      each directory record. I have not implemented this (it's not used,
     *      no?), but if we finally need it, it can be easily implemented in
     *      the iterator, transparently for the rest of the code.
     */
    while ((ret = susp_iter_next(iter, &sue)) > 0) {
        
        /* ignore entries from different version */
        if (sue->version[0] != 1)
            continue; 
        
        if (SUSP_SIG(sue, 'E', 'R')) {
               
            if (data->rr_version) {
                iso_msg_warn(data->messenger, LIBISO_SUSP_MULTIPLE_ER, 
                    "More than one ER has found. This is not supported. "
                    "It will be ignored, but can cause problems. "
                    "Please notify us about this.");
            }

            /* 
             * it seems that Rock Ridge can be identified with any
             * of the following 
             */
            if ( sue->data.ER.len_id[0] == 10 && 
                 !strncmp((char*)sue->data.ER.ext_id, "RRIP_1991A", 10) ) {
                
                iso_msg_debug(data->messenger, 
                              "Suitable Rock Ridge ER found. Version 1.10.");
                data->rr_version = RR_EXT_110;
                
            } else if ( (sue->data.ER.len_id[0] == 10 && 
                    !strncmp((char*)sue->data.ER.ext_id, "IEEE_P1282", 10)) 
                 || (sue->data.ER.len_id[0] == 9 && 
                    !strncmp((char*)sue->data.ER.ext_id, "IEEE_1282", 9)) ) {
                 
                iso_msg_debug(data->messenger, 
                              "Suitable Rock Ridge ER found. Version 1.12.");
                data->rr_version = RR_EXT_112;
                //TODO check also version?
            } else {
                iso_msg_warn(data->messenger, LIBISO_SUSP_MULTIPLE_ER, 
                    "Not Rock Ridge ER found.\n"
                    "That will be ignored, but can cause problems in "
                    "image reading. Please notify us about this");
            }
        }
    }
    
    susp_iter_free(iter);   
    
    if (ret < 0) { 
        return ret;
    }
    
    return ISO_SUCCESS;
}

static
int read_pvm(_ImageFsData *data, uint32_t block)
{
    int ret;
    struct ecma119_pri_vol_desc *pvm;
    struct ecma119_dir_record *rootdr;
    uint8_t buffer[BLOCK_SIZE];

    /* read PVM */
    ret = data->src->read_block(data->src, block, buffer);
    if (ret < 0) {
        return ret;
    }

    pvm = (struct ecma119_pri_vol_desc *)buffer;

    /* sanity checks */
    if (pvm->vol_desc_type[0] != 1 || pvm->vol_desc_version[0] != 1
            || strncmp((char*)pvm->std_identifier, "CD001", 5)
            || pvm->file_structure_version[0] != 1) {

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
    uint32_t block;
    IsoImageFilesystem *ifs;
    _ImageFsData *data;
    uint8_t buffer[BLOCK_SIZE];

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
    data->input_charset = strdup(opts->input_charset);
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
    block = opts->block + 17;
    do {
        ret = src->read_block(src, block, buffer);
        if (ret < 0) {
            /* cleanup and exit */
            goto fs_cleanup;
        }
        switch (buffer[0]) {
        case 0:
            /* 
             * This is a boot record 
             * Here we handle el-torito
             */
            //TODO add support for El-Torito
            iso_msg_hint(data->messenger, LIBISO_UNSUPPORTED_VD,
                         "El-Torito extensions not supported yet");
            break;
        case 2:
            /* suplementary volume descritor */
            // TODO support Joliet
            iso_msg_hint(data->messenger, LIBISO_UNSUPPORTED_VD,
                         "Joliet extensions not supported yet");
            break;
        case 255:
            /* 
             * volume set terminator
             * ignore, as it's checked in loop end condition
             */
            break;
        default:
            {
                iso_msg_hint(data->messenger, LIBISO_UNSUPPORTED_VD,
                             "Ignoring Volume descriptor %x.", buffer[0]);
            }
            break;
        }
        block++;
    } while (buffer[0] != 255);

    /* 4. check if RR extensions are being used */
    ret = read_root_susp_entries(data, data->iso_root_block);
    if (ret < 0) {
        return ret;
    }
    
    /* user doesn't want to read RR extensions */
    if (opts->norock) {
        data->rr = RR_EXT_NO;
    } else {
        data->rr = data->rr_version;
    }

    /* select what tree to read */
    if (data->rr) {
        /* RR extensions are available */
        if (opts->preferjoliet && data->joliet) {
            /* if user prefers joliet, that is used */
            iso_msg_debug(data->messenger, "Reading Joliet extensions.");
            //data->get_name = ucs2str;
            data->rr = RR_EXT_NO;
            //data->root_dir_block = joliet root
            /* root_dir_block already contains root for joliet */
            //TODO add joliet support
            goto fs_cleanup;
        } else {
            /* RR will be used */
            iso_msg_debug(data->messenger, "Reading Rock Ridge extensions.");
            //data->root_dir_block = info.iso_root_block;
            data->get_name = strcopy;
        }
    } else {
        /* RR extensions are not available */
        if (opts->preferjoliet && data->joliet) {
            /* joliet will be used */
            iso_msg_debug(data->messenger, "Reading Joliet extensions.");
            //data->get_name = ucs2str;
            //data->root_dir_block = joliet root
            /* root_dir_block already contains root for joliet */
            //TODO add joliet support
            goto fs_cleanup;
        } else {
            /* default to plain iso */
            iso_msg_debug(data->messenger, "Reading plain ISO-9660 tree.");
            //data->root_dir_block = info.iso_root_block;
            data->get_name = strcopy;
        }
    }

    /* and finally return. Note that we keep the DataSource opened */

    *fs = ifs;
    return ISO_SUCCESS;

    fs_cleanup: ;
    ifs_fs_free((IsoFilesystem*)ifs);
    free(ifs);
    return ret;
}
