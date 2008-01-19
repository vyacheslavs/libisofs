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

struct libiso_msgs;
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
                             int msgid, IsoImageFilesystem **fs);

const char *iso_image_fs_get_volset_id(IsoImageFilesystem *fs);

const char *iso_image_fs_get_volume_id(IsoImageFilesystem *fs);

const char *iso_image_fs_get_publisher_id(IsoImageFilesystem *fs);

const char *iso_image_fs_get_data_preparer_id(IsoImageFilesystem *fs);

const char *iso_image_fs_get_system_id(IsoImageFilesystem *fs);

const char *iso_image_fs_get_application_id(IsoImageFilesystem *fs);

const char *iso_image_fs_get_copyright_file_id(IsoImageFilesystem *fs);

const char *iso_image_fs_get_abstract_file_id(IsoImageFilesystem *fs);

const char *iso_image_fs_get_biblio_file_id(IsoImageFilesystem *fs);

#endif /*LIBISO_FS_IMAGE_H_*/
