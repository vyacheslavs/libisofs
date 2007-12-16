/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_ECMA119_TREE_H_
#define LIBISO_ECMA119_TREE_H_

#include "libisofs.h"
#include "ecma119.h"

enum ecma119_node_type {
    ECMA119_FILE,
    ECMA119_DIR
};

/**
 * Struct with info about a node representing a tree
 */
struct ecma119_dir_info {
    /* Block where the directory entries will be written on image */
    size_t block;

    size_t nchildren;
    Ecma119Node **children;
};

/**
 * A node for a tree containing all the information necessary for writing
 * an ISO9660 volume.
 */
struct ecma119_node
{
    /**
     * Name in ASCII, conforming to selected ISO level.
     * Version number is not include, it is added on the fly
     */
    char *iso_name;
    
    Ecma119Node *parent;
    
    IsoNode *node; /*< reference to the iso node */

    /**< file, symlink, directory or placeholder */
    enum ecma119_node_type type; 
    union {
        IsoFileSrc *file;
        struct ecma119_dir_info dir;
    } info;
};

/**
 * 
 */
int ecma119_tree_create(Ecma119Image *img, IsoDir *iso);

/**
 * Free an Ecma119Node, and its children if node is a dir
 */
void ecma119_node_free(Ecma119Node *node);

#endif /*LIBISO_ECMA119_TREE_H_*/
