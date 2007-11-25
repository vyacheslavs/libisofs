/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_IMAGE_H_
#define LIBISO_IMAGE_H_


struct IsoImage {
	
	/*
	 * Image is a context for image manipulation.
	 * Global objects such as the message_queues must belogn to that
	 * context. Thus we will have, for example, a msg queue per image,
	 * so images are completelly independent and can be managed together.
	 * (Usefull, for example, in Multiple-Document-Interface GUI apps.
	 * [The stuff we have in init belongs really to image!]
	 */
	
};

#endif /*LIBISO_IMAGE_H_*/
