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
#include "image.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

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
int default_create_file(IsoNodeBuilder *builder, IsoImage *image,
                        IsoFileSource *src, IsoFile **file)
{
    int res;
    struct stat info;
    IsoStream *stream;
    IsoFile *node;

    if (builder == NULL || src == NULL || file == NULL) {
        return ISO_NULL_POINTER;
    }

    res = iso_file_source_stat(src, &info);
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
    node->node.name = iso_file_source_get_name(src);
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

static
int default_create_node(IsoNodeBuilder *builder, IsoImage *image,
                        IsoFileSource *src, IsoNode **node)
{
    int result;
    struct stat info;
    IsoNode *new;
    char *name;

    if (builder == NULL || src == NULL || node == NULL) {
        return ISO_NULL_POINTER;
    }

    name = iso_file_source_get_name(src);

    /* get info about source */
    if (image->recOpts->follow_symlinks) {
        result = iso_file_source_stat(src, &info);
    } else {
        result = iso_file_source_lstat(src, &info);
    }
    if (result < 0) {
        return result;
    }

    new = NULL;
    switch (info.st_mode & S_IFMT) {
    case S_IFREG:
        {
            /* source is a regular file */
            IsoStream *stream;
            IsoFile *file;
            result = iso_file_source_stream_new(src, &stream);
            if (result < 0) {
                free(name);
                return result;
            }
            /* take a ref to the src, as stream has taken our ref */
            iso_file_source_ref(src);
            file = calloc(1, sizeof(IsoFile));
            if (file == NULL) {
                free(name);
                iso_stream_unref(stream);
                return ISO_MEM_ERROR;
            }
            file->msblock = 0;
            file->sort_weight = 0;
            file->stream = stream;
            file->node.type = LIBISO_FILE;
            new = (IsoNode*) file;
        }
        break;
    case S_IFDIR:
        {
            /* source is a directory */
            new = calloc(1, sizeof(IsoDir));
            if (new == NULL) {
                free(name);
                return ISO_MEM_ERROR;
            }
            new->type = LIBISO_DIR;
        }
        break;
    case S_IFLNK:
        {
            /* source is a symbolic link */
            char dest[PATH_MAX];
            IsoSymlink *link;

            result = iso_file_source_readlink(src, dest, PATH_MAX);
            if (result < 0) {
                free(name);
                return result;
            }
            link = malloc(sizeof(IsoSymlink));
            if (link == NULL) {
                free(name);
                return ISO_MEM_ERROR;
            }
            link->dest = strdup(dest);
            link->node.type = LIBISO_SYMLINK;
            new = (IsoNode*) link;
        }
        break;
    case S_IFSOCK:
    case S_IFBLK:
    case S_IFCHR:
    case S_IFIFO:
        {
            /* source is an special file */
            IsoSpecial *special;
            special = malloc(sizeof(IsoSpecial));
            if (special == NULL) {
                free(name);
                return ISO_MEM_ERROR;
            }
            special->dev = info.st_rdev;
            special->node.type = LIBISO_SPECIAL;
            new = (IsoNode*) special;
        }
        break;
    }

    /* fill fields */
    new->refcount = 1;
    new->name = name;
    new->mode = info.st_mode;
    new->uid = info.st_uid;
    new->gid = info.st_gid;
    new->atime = info.st_atime;
    new->mtime = info.st_mtime;
    new->ctime = info.st_ctime;

    new->hidden = 0;

    new->parent = NULL;
    new->next = NULL;

    *node = new;
    return ISO_SUCCESS;
}

static
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
    b->create_file_data = NULL;
    b->create_node_data = NULL;
    b->create_file = default_create_file;
    b->create_node = default_create_node;
    b->free = default_free;

    *builder = b;
    return ISO_SUCCESS;
}
