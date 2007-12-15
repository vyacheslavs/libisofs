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

#include <stdlib.h>
#include <search.h>

static
int comp_iso_file_src(const void *n1, const void *n2)
{
    const IsoFileSrc *f1, *f2;
    f1 = (const IsoFileSrc *)n1;
    f2 = (const IsoFileSrc *)n2;
    
    if (f1->fs_id < f2->fs_id) {
        return -1;
    } else if (f1->fs_id > f2->fs_id) {
        return 1;
    } else {
        /* files belong to the same fs */
        if (f1->dev_id > f2->dev_id) {
            return -1;
        } else if (f1->dev_id < f2->dev_id) {
            return 1;
        } else {
            /* files belong to same device in same fs */
            return (f1->ino_id < f2->ino_id) ? -1 :
                    (f1->ino_id > f2->ino_id) ? 1 : 0;
        }
    }
}

int iso_file_src_create(Ecma119Image *img, IsoFile *file, IsoFileSrc **src)
{
    int res;
    IsoStream *stream;
    IsoFileSrc *fsrc;
    unsigned int fs_id;
    dev_t dev_id;
    ino_t ino_id;
    off_t size;
    
    if (img == NULL || file == NULL || src == NULL) {
        return ISO_NULL_POINTER;
    }
    
    stream = file->stream;
    res = stream->get_id(stream, &fs_id, &dev_id, &ino_id);
    if (res < 0) {
        return res;
    } else if (res == 0) {
        // TODO this corresponds to Stream that cannot provide a valid id
        // Not implemented for now
        return ISO_ERROR;
    } else {
        IsoFileSrc **inserted;
        
        size = stream->get_size(stream);
        if (size == (off_t)-1) {
            return ISO_FILE_ERROR;
        }
        
        fsrc = malloc(sizeof(IsoFileSrc));
        if (fsrc == NULL) {
            return ISO_MEM_ERROR;
        }
        
        /* fill key and other atts */
        fsrc->fs_id = fs_id;
        fsrc->dev_id = dev_id;
        fsrc->ino_id = ino_id;
        fsrc->prev_img = file->msblock ? 1 : 0;
        fsrc->block = file->msblock;
        fsrc->size = size;
        fsrc->sort_weight = file->sort_weight;
        fsrc->stream = file->stream;
        
        /* insert the filesrc in the tree */
        inserted = tsearch(fsrc, &(img->file_srcs), comp_iso_file_src);
        if (inserted == NULL) {
            free(fsrc);
            return ISO_MEM_ERROR;
        } else if (*inserted == fsrc) {
            /* the file was inserted */
            img->file_count++;
        } else {
            /* the file was already on the tree */
            free(fsrc);
        }
        *src = *inserted;
    }
    return ISO_SUCCESS;
}

