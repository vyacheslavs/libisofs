/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_FS_IMAGE_H_
#define LIBISO_FS_IMAGE_H_

#include "libisofs.h"
#include "fsource.h"

/**
 * Options for image reading.
 * There are four kind of options:
 * - Related to multisession support.
 *   In most cases, an image begins at LBA 0 of the data source. However,
 *   in multisession discs, the later image begins in the last session on
 *   disc. The block option can be used to specify the start of that last
 *   session.
 * - Related to the tree that will be read.
 *   As default, when Rock Ridge extensions are present in the image, that
 *   will be used to get the tree. If RR extensions are not present, libisofs
 *   will use the Joliet extensions if available. Finally, the plain ISO-9660
 *   tree is used if neither RR nor Joliet extensions are available. With
 *   norock, nojoliet, and preferjoliet options, you can change this
 *   default behavior.
 * - Related to default POSIX attributes.
 *   When Rock Ridege extensions are not used, libisofs can't figure out what
 *   are the the permissions, uid or gid for the files. You should supply
 *   default values for that.
 * - Return information for image.
 *   Both size, hasRR and hasJoliet will be filled by libisofs with suitable values.
 *   Also, error is set to non-0 if some error happens (error codes are
 *   private now)
 */
struct iso_read_opts
{
    /** 
     * Block where the image begins, usually 0, can be different on a 
     * multisession disc.
     */
    uint32_t block; 

    unsigned int norock : 1; /*< Do not read Rock Ridge extensions */
    unsigned int nojoliet : 1; /*< Do not read Joliet extensions */

    /** 
     * When both Joliet and RR extensions are present, the RR tree is used. 
     * If you prefer using Joliet, set this to 1. 
     */
    unsigned int preferjoliet : 1; 
    
    uid_t uid; /**< Default uid when no RR */
    gid_t gid; /**< Default uid when no RR */
    mode_t mode; /**< Default mode when no RR (only permissions) */
    //TODO differ file and dir mode
    //option to convert names to lower case?

    struct libiso_msgs *messenger;
    
    char *input_charset;

/* modified by the function */
//    unsigned int hasRR:1; /*< It will be set to 1 if RR extensions are present,
//                              to 0 if not. */
//    unsigned int hasJoliet:1; /*< It will be set to 1 if Joliet extensions are 
//                                  present, to 0 if not. */
//    uint32_t size; /**< Will be filled with the size (in 2048 byte block) of 
//                    *   the image, as reported in the PVM. */
//int error;
};

typedef struct Iso_Image_Filesystem IsoImageFilesystem;

/**
 * Extends IsoFilesystem interface, to offer a way to access specific 
 * information of the image, such as several volume attributes, extensions
 * being used, El-Torito artifacts...
 */
struct Iso_Image_Filesystem
{
    IsoFilesystem fs;

    /*
     * TODO both open and close have meaning to other filesystems, in fact
     * they seem useful for any kind of Filesystems, with the exception of
     * the local filesystem. Thus, we should consider adding them to
     * IsoFilesystem interface
     */

    /**
     * Opens the filesystem for several read operations. Calling this funcion
     * is not needed at all, each time that the underlying IsoDataSource need
     * to be read, it is openned propertly. However, if you plan to execute
     * several operations on the image, it is a good idea to open it 
     * previously, to prevent several open/close operations. 
     */
    int (*open)(IsoImageFilesystem *fs);

    int (*close)(IsoImageFilesystem *fs);
};

int iso_image_filesystem_new(IsoDataSource *src, struct iso_read_opts *opts,
                             IsoImageFilesystem **fs);

#endif /*LIBISO_FS_IMAGE_H_*/
