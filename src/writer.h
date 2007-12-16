/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_IMAGE_WRITER_H_
#define LIBISO_IMAGE_WRITER_H_

#include "ecma119.h"

struct Iso_Image_Writer
{
    /**
     * 
     */
    int (*compute_data_blocks)(IsoImageWriter *writer);
    
    int (*write_vol_desc)(IsoImageWriter *writer, int fd);
    
    int (*write_data)(IsoImageWriter *writer, int fd);
    
    int (*free_data)(IsoImageWriter *writer);
    
    void *data;
    Ecma119Image *image;
    IsoImageWriter *next;
};

#endif /*LIBISO_IMAGE_WRITER_H_*/
