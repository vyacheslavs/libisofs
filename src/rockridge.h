/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2007 Mario Danic
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

/** 
 * Functions and structures used for Rock Ridge support.
 * 
 * See IEEE P1282, Rock Ridge Interchange Protocol, Draft Standard version
 * 1.12 for further details.
 */

#ifndef LIBISO_ROCKRIDGE_H
#define LIBISO_ROCKRIDGE_H

#include "ecma119.h"

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

#endif /* LIBISO_ROCKRIDGE_H */
