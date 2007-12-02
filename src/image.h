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
    
/*
 * Image is a context for image manipulation.
 * Global objects such as the message_queues must belogn to that
 * context. Thus we will have, for example, a msg queue per image,
 * so images are completelly independent and can be managed together.
 * (Usefull, for example, in Multiple-Document-Interface GUI apps.
 * [The stuff we have in init belongs really to image!]
 */

struct Iso_Image {
	
    int refcount;
    
    IsoDir *root;
    
    char *volset_id;
    
    char *volume_id;        /**< Volume identifier. */
    char *publisher_id;     /**< Volume publisher. */
    char *data_preparer_id; /**< Volume data preparer. */
    char *system_id;        /**< Volume system identifier. */
    char *application_id;   /**< Volume application id */
    char *copyright_file_id;
    char *abstract_file_id;
    char *biblio_file_id;
    
    /* message messenger for the image */
    struct libiso_msgs *messenger;
};

#endif /*LIBISO_IMAGE_H_*/
