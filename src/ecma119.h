/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_ECMA119_H_
#define LIBISO_ECMA119_H_

#include "libisofs.h"

#include <stdint.h>

typedef struct ecma119_image Ecma119Image;
typedef struct ecma119_node Ecma119Node;
typedef struct Iso_File_Src IsoFileSrc;
typedef struct Iso_Image_Writer IsoImageWriter;

struct ecma119_image {
    IsoImage *image;
    Ecma119Node *root;
    
    unsigned int iso_level:2;
    
//    int relaxed_constraints; /**< see ecma119_relaxed_constraints_flag */
//    
//    int replace_mode; /**< Replace ownership and modes of files
//                      * 
//                      * 0. filesystem values
//                      * 1. useful values
//                      * bits 1-4 bitmask: 
//                      *     2 - replace dir
//                      *     3 - replace file 
//                      *     4 - replace gid
//                      *     5 - replace uid
//                      */
//    mode_t dir_mode;
//    mode_t file_mode;
//    gid_t gid;
//    uid_t uid;
    
    
    time_t now;     /**< Time at which writing began. */
    off_t total_size; /**< Total size of the output. This only
                        *  includes the current volume. */
    //uint32_t vol_space_size;

    /*
     * Block being processed, either during image writing or structure
     * size calculation.
     */
    uint32_t curblock;
    
    size_t nwriters;
    IsoImageWriter **writers;

    /* tree of files sources */
    void *file_srcs;
    int file_count;
};

#endif /*LIBISO_ECMA119_H_*/
