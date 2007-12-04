/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "builder.h"
#include "error.h"
#include "node.h"
#include "fsource.h"

#include <stdlib.h>
#include <string.h>

void iso_node_builder_ref(IsoNodeBuilder *builder)
{
    ++builder->refcount;
}

void iso_node_builder_unref(IsoNodeBuilder *builder)
{
    if (--builder->refcount == 0) {
        /* free private data */
        builder->free(builder);
        free(builder);
    }
}

static
int default_create_file(IsoNodeBuilder *builder, IsoFileSource *src, 
                        IsoFile **file)
{
    int res;
    struct stat info;
    IsoStream *stream;
    IsoFile *node;
    
    if (builder == NULL || src == NULL || file == NULL) {
        return ISO_NULL_POINTER;
    }
    
    res = src->stat(src, &info);
    if (res < 0) {
        return res;
    }
    
    /* this will fail if src is a dir, is not accessible... */
    res = iso_file_source_stream_new(src, &stream);
    if (res < 0) {
        return res;
    }
    
    node = malloc(sizeof(IsoFile));
    if (node == NULL) {
        /* the stream has taken our ref to src, so we need to add one */
        iso_file_source_ref(src);
        iso_stream_unref(stream);
        return ISO_MEM_ERROR;
    }
    
    /* fill node fields */
    node->node.refcount = 1;
    node->node.type = LIBISO_FILE;
    node->node.name = strdup(src->get_name(src));
    node->node.mode = S_IFREG | (info.st_mode & ~S_IFMT);
    node->node.uid = info.st_uid;
    node->node.gid = info.st_gid;
    node->node.atime = info.st_atime;
    node->node.ctime = info.st_ctime;
    node->node.mtime = info.st_mtime;
    node->node.hidden = 0;
    node->node.parent = NULL;
    node->node.next = NULL;
    node->sort_weight = 0;
    node->stream = stream;
    node->msblock = 0;
    
    *file = node;
    return ISO_SUCCESS;
}

void default_free(IsoNodeBuilder *builder)
{
    return;
}

int iso_node_basic_builder_new(IsoNodeBuilder **builder)
{
    IsoNodeBuilder *b;
    
    if (builder == NULL) {
        return ISO_NULL_POINTER;
    }
    
    b = malloc(sizeof(IsoNodeBuilder));
    if (b == NULL) {
        return ISO_MEM_ERROR;
    }
    
    b->refcount = 1;
    b->data = NULL;
    b->create_file = default_create_file;
    b->free = default_free;
    
    *builder = b;
    return ISO_SUCCESS;
}
