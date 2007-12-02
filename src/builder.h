/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_BUILDER_H_
#define LIBISO_BUILDER_H_

/*
 * Definitions for IsoNode builders.
 */

/*
 * Some functions here will be moved to libisofs.h when we expose 
 * Builder.
 */

#include "node.h"

typedef struct Iso_Node_Builder IsoNodeBuilder;

struct Iso_Node_Builder
{

    /**
     * 
     * @return
     *    1 on success, < 0 on error
     */
    int (*create_file)(const IsoFileSource *src, IsoFile **file);
    
    
    
    int refcount;
    void *data;
};



#endif /*LIBISO_BUILDER_H_*/
