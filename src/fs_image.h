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
 * IsoFilesystem implementation to deal with ISO images, and to offer a way to
 * access specific information of the image, such as several volume attributes, 
 * extensions being used, El-Torito artifacts...
 */

struct libiso_msgs;
typedef IsoFilesystem IsoImageFilesystem;

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
