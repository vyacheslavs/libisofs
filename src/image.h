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

typedef struct Iso_Image_Rec_Opts IsoImageRecOpts;

/**
 * Options for recursive directory addition
 */
struct Iso_Image_Rec_Opts
{

    /**
     * Whether to follow symlinks or just add them as symlinks
     */
    unsigned int follow_symlinks : 1;

    /**
     * Whether to skip hidden files
     */
    unsigned int ignore_hidden : 1;

    /**
     * Whether to stop on an error. Some errors, such as memory errors,
     * always cause a stop
     */
    unsigned int stop_on_error : 1;

    /**
     * Files to exclude
     * TODO add wildcard support
     */
    char** excludes;

    /**
     * if the dir already contains a node with the same name, whether to
     * replace or not the old node with the new. 
     */
    enum iso_replace_mode replace;

    /**
     * When this is not NULL, it is a pointer to a function that will
     * be called just before a file will be added, or when an error occurs.
     * You can overwrite some of the above options by returning suitable 
     * values.
     * 
     * @param action
     *      1 file will be added
     *      2 file will be skipped
     *      < 0 error adding file (return 3 to stop, 1 to continue)
     * @param flag
     *      0 no problem
     *      1 file with same name already exists
     * @return
     *      1 add/continue, 2 skip, 3 stop
     */
    int (*report)(IsoFileSource *src, int action, int flag);
};

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

    /* message messenger for the image */
    struct libiso_msgs *messenger;

    /**
     * Default filesystem to use when adding files to the image tree.
     */
    IsoFilesystem *fs;

    /*
     * Default builder to use when adding files to the image tree.
     */
    IsoNodeBuilder *builder;

    /**
     * Options for recursive directory addition
     */
    IsoImageRecOpts recOpts;
};

#endif /*LIBISO_IMAGE_H_*/
