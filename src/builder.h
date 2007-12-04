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

#include "libisofs.h"
#include "fsource.h"

typedef struct Iso_Node_Builder IsoNodeBuilder;

struct Iso_Node_Builder
{

    /**
     * Create a new IsoFile from an IsoFileSource. Name, permissions
     * and other attributes are taken from src, but a regular file will
     * always be created, even if src is another kind of file.
     * 
     * In that case, if the implementation can't do the conversion, it
     * should fail propertly.
     * 
     * On sucess, the ref. to src will be owned by file, so you musn't
     * unref it.
     * 
     * @return
     *    1 on success, < 0 on error
     */
    int (*create_file)(IsoNodeBuilder *builder, IsoFileSource *src, 
                       IsoFile **file);
    
    /**
     * Free implementation specific data. Should never be called by user.
     * Use iso_node_builder_unref() instead.
     */
    void (*free)(IsoNodeBuilder *builder);
    
    int refcount;
    void *data;
};

void iso_node_builder_ref(IsoNodeBuilder *builder);
void iso_node_builder_unref(IsoNodeBuilder *builder);

/**
 * Create a new basic builder ...
 * 
 * @return
 *     1 success, < 0 error
 */
int iso_node_basic_builder_new(IsoNodeBuilder **builder);

#endif /*LIBISO_BUILDER_H_*/
