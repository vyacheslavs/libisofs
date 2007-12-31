/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

/*
 * This file contains functions related to the reading of SUSP and
 * Rock Ridge extensions on an ECMA-119 image.
 */

#include "libisofs.h"
#include "ecma119.h"
#include "util.h"
#include "rockridge.h"
#include "error.h"

#include <sys/stat.h>
#include <stdlib.h>

struct susp_iterator
{
    uint8_t* base;
    int pos;
    int size;
    IsoDataSource *src;
    IsoMessenger *msgr;

    /* block and offset for next continuation area */
    uint32_t ce_block;
    uint32_t ce_off;
    
    /** Length of the next continuation area, 0 if no more CA are specified */
    uint32_t ce_len; 

    uint8_t *buffer; /*< If there are continuation areas */
};

SuspIterator*
susp_iter_new(IsoDataSource *src, struct ecma119_dir_record *record,
              uint8_t len_skp, IsoMessenger *msgr)
{
    int pad = (record->len_fi[0] + 1) % 2;
    struct susp_iterator *iter = malloc(sizeof(struct susp_iterator));
    if (iter == NULL) {
        return NULL;
    }

    iter->base = record->file_id + record->len_fi[0] + pad;
    iter->pos = len_skp; /* 0 in most cases */
    iter->size = record->len_dr[0] - record->len_fi[0] - 33 - pad;
    iter->src = src;
    iter->msgr = msgr;

    iter->ce_len = 0;
    iter->buffer = NULL;

    return iter;
}

int susp_iter_next(SuspIterator *iter, struct susp_sys_user_entry **sue)
{
    struct susp_sys_user_entry *entry;

    entry = (struct susp_sys_user_entry*)(iter->base + iter->pos);

    if ( (iter->pos + 4 > iter->size) || (SUSP_SIG(entry, 'S', 'T'))) {

        /* 
         * End of the System Use Area or Continuation Area.
         * Note that ST is not needed when the space left is less than 4.
         * (IEEE 1281, SUSP. section 4) 
         */
        if (iter->ce_len) {
            uint32_t block;
            int nblocks;

            /* A CE has found, there is another continuation area */
            nblocks = div_up(iter->ce_off + iter->ce_len, BLOCK_SIZE);
            iter->buffer = realloc(iter->buffer, nblocks * BLOCK_SIZE);

            /* read all blocks needed to cache the full CE */
            for (block = 0; block < nblocks; ++block) {
                int ret;
                ret = iter->src->read_block(iter->src, iter->ce_block + block,
                                            iter->buffer + block * BLOCK_SIZE);
                if (ret < 0) {
                    return ret;
                }
            }
            iter->base = iter->buffer + iter->ce_off;
            iter->pos = 0;
            iter->size = iter->ce_len;
            iter->ce_len = 0;
            entry = (struct susp_sys_user_entry*)iter->base;
        } else {
            return 0;
        }
    }

    if (entry->len_sue[0] == 0) {
        /* a wrong image with this lead us to a infinity loop */
        iso_msg_sorry(iter->msgr, LIBISO_RR_ERROR,
                      "Damaged RR/SUSP information.");
        return ISO_WRONG_RR;
    }

    iter->pos += entry->len_sue[0];

    if (SUSP_SIG(entry, 'C', 'E')) {
        /* Continuation entry */
        if (iter->ce_len) {
            iso_msg_sorry(iter->msgr, LIBISO_RR_ERROR, "More than one CE "
                "System user entry has found in a single System Use field or "
                "continuation area. This breaks SUSP standard and it's not "
                "supported. Ignoring last CE. Maybe the image is damaged.");
        } else {
            iter->ce_block = iso_read_bb(entry->data.CE.block, 4, NULL);
            iter->ce_off = iso_read_bb(entry->data.CE.offset, 4, NULL);
            iter->ce_len = iso_read_bb(entry->data.CE.len, 4, NULL);
        }

        /* we don't want to return CE entry to the user */
        return susp_iter_next(iter, sue);
    } else if (SUSP_SIG(entry, 'P', 'D')) {
        /* skip padding */
        return susp_iter_next(iter, sue);
    }

    *sue = entry;
    return ISO_SUCCESS;
}

void susp_iter_free(SuspIterator *iter)
{
    free(iter->buffer);
    free(iter);
}
