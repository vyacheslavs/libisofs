/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2007 Mario Danic
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

/**
 * This header defines the functions and structures needed to add RockRidge
 * extensions to an ISO image. 
 * 
 * References:
 * 
 * - SUSP (IEEE 1281).
 * System Use Sharing Protocol, draft standard version 1.12.
 * 
 * - RRIP (IEEE 1282)
 * Rock Ridge Interchange Protocol, Draft Standard version 1.12.
 * 
 * - ECMA-119 (ISO-9660)
 * Volume and File Structure of CDROM for Information Interchange.
 */

#ifndef LIBISO_ROCKRIDGE_H
#define LIBISO_ROCKRIDGE_H

#include "ecma119.h"


/**
 * This contains the information about the System Use Fields (SUSP, 4.1), 
 * that will be written in the System Use Areas, both in the ISO directory
 * record System Use field (ECMA-119, 9.1.13) or in a Continuation Area as
 * defined by SUSP.
 */
struct susp_info
{
    /** Number of SUSP fields in the System Use field */
    size_t n_susp_fields;
    uint8_t **susp_fields;

    /** Length of the part of the SUSP area that fits in the dirent. */
    int suf_len;

    /** Length of the part of the SUSP area that will go in a CE area. */
    uint32_t ce_block;
    uint32_t ce_len;

    size_t n_ce_susp_fields;
    uint8_t **ce_susp_fields;
};

/**
 * Compute the length needed for write all RR and SUSP entries for a given
 * node.
 * 
 * @param type
 *      0 normal entry, 1 "." entry for that node (it is a dir), 2 ".."
 *      for that node (i.e., it will refer to the parent)
 * @param space
 *      Available space in the System Use Area for the directory record.
 * @param ce
 *      Will be filled with the space needed in a CE
 * @return
 *      The size needed for the RR entries in the System Use Area
 */
size_t rrip_calc_len(Ecma119Image *t, Ecma119Node *n, int type,
                     size_t space, size_t *ce);

/**
 * Fill a struct susp_info with the RR/SUSP entries needed for a given
 * node.
 * 
 * @param type
 *      0 normal entry, 1 "." entry for that node (it is a dir), 2 ".."
 *      for that node (i.e., it will refer to the parent)
 * @param space
 *      Available space in the System Use Area for the directory record.
 * @param info
 *      Pointer to the struct susp_info where the entries will be stored.
 *      If some entries need to go to a Continuation Area, they will be added
 *      to the existing ce_susp_fields, and ce_len will be incremented
 *      propertly. Please ensure ce_block is initialized propertly.
 */
int rrip_get_susp_fields(Ecma119Image *t, Ecma119Node *n, int type, 
                         size_t space, struct susp_info *info);

/**
 * Write the given SUSP fields into buf. Note that Continuation Area
 * fields are not written.
 * If info does not contain any SUSP entry this function just return. 
 * After written, the info susp_fields array will be freed, and the counters
 * updated propertly.
 */
void rrip_write_susp_fields(Ecma119Image *t, struct susp_info *info, 
                            void *buf);

/**
 * Write the Continuation Area entries for the given struct susp_info, using
 * the iso_write() function.
 * After written, the ce_susp_fields array will be freed.
 */
int rrip_write_ce_fields(Ecma119Image *t, struct susp_info *info);

#endif /* LIBISO_ROCKRIDGE_H */
