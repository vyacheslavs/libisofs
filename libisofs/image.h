/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_IMAGE_H_
#define LIBISO_IMAGE_H_

#include "libisofs.h"
#include "node.h"
#include "fsource.h"
#include "builder.h"

/*
 * Image is a context for image manipulation.
 * Global objects such as the message_queues must belogn to that
 * context. Thus we will have, for example, a msg queue per image,
 * so images are completelly independent and can be managed together.
 * (Usefull, for example, in Multiple-Document-Interface GUI apps.
 * [The stuff we have in init belongs really to image!]
 */

struct Iso_Image
{

    int refcount;

    IsoDir *root;

    char *volset_id;

    char *volume_id; /**< Volume identifier. */
    char *publisher_id; /**< Volume publisher. */
    char *data_preparer_id; /**< Volume data preparer. */
    char *system_id; /**< Volume system identifier. */
    char *application_id; /**< Volume application id */
    char *copyright_file_id;
    char *abstract_file_id;
    char *biblio_file_id;
    
    /* el-torito boot catalog */
    struct el_torito_boot_catalog *bootcat;

    /* image identifier, for message origin identifier */
    int id;

    /**
     * Default filesystem to use when adding files to the image tree.
     */
    IsoFilesystem *fs;

    /*
     * Default builder to use when adding files to the image tree.
     */
    IsoNodeBuilder *builder;

    /**
     * Whether to follow symlinks or just add them as symlinks
     */
    unsigned int follow_symlinks : 1;

    /**
     * Whether to skip hidden files
     */
    unsigned int ignore_hidden : 1;

    /**
     * Flags that determine what special files should be ignore. It is a
     * bitmask:
     * bit0: ignore FIFOs
     * bit1: ignore Sockets
     * bit2: ignore char devices
     * bit3: ignore block devices
     */
    int ignore_special;

    /**
     * Files to exclude. Wildcard support is included.
     */
    char** excludes;
    int nexcludes;

    /**
     * if the dir already contains a node with the same name, whether to
     * replace or not the old node with the new. 
     */
    enum iso_replace_mode replace;

    /* TODO
    enum iso_replace_mode (*confirm_replace)(IsoFileSource *src, IsoNode *node);
    */
    
    /**
     * When this is not NULL, it is a pointer to a function that will
     * be called just before a file will be added. You can control where
     * the file will be in fact added or ignored.
     * 
     * @return
     *      1 add, 0 ignore, < 0 cancel
     */
    int (*report)(IsoImage *image, IsoFileSource *src);

    /**
     * User supplied data
     */
    void *user_data;
    void (*user_data_free)(void *ptr);
};

#endif /*LIBISO_IMAGE_H_*/
