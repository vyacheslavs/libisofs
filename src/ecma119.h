/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_ECMA119_H_
#define LIBISO_ECMA119_H_

typedef struct ecma119_image Ecma119Image;

struct ecma119_image {
    
    unsigned int iso_level:2;

    /* tree of files sources */
    void *file_srcs;
    int file_count;
};

#endif /*LIBISO_ECMA119_H_*/
