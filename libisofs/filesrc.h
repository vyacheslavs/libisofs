/*
 * Copyright (c) 2007 Vreixo Formoso
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 
 * or later as published by the Free Software Foundation. 
 * See COPYING file for details.
 */
#ifndef LIBISO_FILESRC_H_
#define LIBISO_FILESRC_H_

#include "libisofs.h"
#include "stream.h"
#include "ecma119.h"

#include <stdint.h>

struct Iso_File_Src
{
    unsigned int prev_img :1; /**< if the file comes from a previous image */

#ifdef Libisofs_with_checksumS

    unsigned int checksum_index :31;

#endif /* Libisofs_with_checksumS */


    /** File Sections of the file in the image */
    struct iso_file_section *sections;
    int nsections;

    int sort_weight;
    IsoStream *stream;
};

int iso_file_src_cmp(const void *n1, const void *n2);

/**
 * Create a new IsoFileSrc to get data from a specific IsoFile.
 *
 * The IsoFileSrc will be cached in a tree to prevent the same file for
 * being written several times to image. If you call again this function
 * with a node that refers to the same source file, the previously
 * created one will be returned. No new IsoFileSrc is created in that case.
 *
 * @param img
 *      The image where this file is to be written
 * @param file
 *      The IsoNode we want to write
 * @param src
 *      Will be filled with a pointer to the IsoFileSrc
 * @return
 *      1 if new object was created, 0 if object existed, < 0 on error
 */
int iso_file_src_create(Ecma119Image *img, IsoFile *file, IsoFileSrc **src);

/**
 * Add a given IsoFileSrc to the given image target.
 *
 * The IsoFileSrc will be cached in a tree to prevent the same file for
 * being written several times to image. If you call again this function
 * with a node that refers to the same source file, the previously
 * created one will be returned.
 *
 * @param img
 *      The image where this file is to be written
 * @param new
 *      The IsoFileSrc to add
 * @param src
 *      Will be filled with a pointer to the IsoFileSrc really present in
 *      the tree. It could be different than new if the same file already
 *      exists in the tree.
 * @return
 *      1 on success, 0 if file already exists on tree, < 0 error
 */
int iso_file_src_add(Ecma119Image *img, IsoFileSrc *new, IsoFileSrc **src);

/**
 * Free the IsoFileSrc especific data
 */
void iso_file_src_free(void *node);

/**
 * Get the size of the file this IsoFileSrc represents
 */
off_t iso_file_src_get_size(IsoFileSrc *file);

/**
 * Create a Writer for file contents.
 *
 * It takes care of written the files in the correct order.
 */
int iso_file_src_writer_create(Ecma119Image *target);

#endif /*LIBISO_FILESRC_H_*/
