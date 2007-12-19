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

#include <stdlib.h>

int iso_file_src_cmp(const void *n1, const void *n2)
{
    const IsoFileSrc *f1, *f2;
    int res;
    unsigned int fs_id1, fs_id2;
    dev_t dev_id1, dev_id2;
    ino_t ino_id1, ino_id2;
    
    f1 = (const IsoFileSrc *)n1;
    f2 = (const IsoFileSrc *)n2;
    
    res = f1->stream->get_id(f1->stream, &fs_id1, &dev_id1, &ino_id1);
    res = f2->stream->get_id(f2->stream, &fs_id2, &dev_id2, &ino_id2);
    
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
            return (ino_id1 < ino_id2) ? -1 :
                    (ino_id1 > ino_id2) ? 1 : 0;
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
    
    if (img == NULL || file == NULL || src == NULL) {
        return ISO_NULL_POINTER;
    }
    
    stream = file->stream;
    res = stream->get_id(stream, &fs_id, &dev_id, &ino_id);
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
    return file->stream->get_size(file->stream);
}
