/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_NODE_H_
#define LIBISO_NODE_H_

/*
 * Definitions for the public iso tree
 */

#include "libisofs.h"
#include "stream.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

/**
 * 
 */
struct Iso_Node
{
    /*
     * Initilized to 1, originally owned by user, until added to another node.
     * Then it is owned by the parent node, so the user must take his own ref 
     * if needed. With the exception of the creation functions, none of the
     * other libisofs functions that return an IsoNode increment its 
     * refcount. This is responsablity of the client, if (s)he needs it.
     */
    int refcount;

    /**< Type of the IsoNode, do not confuse with mode */
    enum IsoNodeType type;

    char *name; /**< Real name, supossed to be in UTF-8 */

    mode_t mode; /**< protection */
    uid_t uid;   /**< user ID of owner */
    gid_t gid;   /**< group ID of owner */

    /* TODO #00001 : consider adding new timestamps */
    time_t atime; /**< time of last access */
    time_t mtime; /**< time of last modification */
    time_t ctime; /**< time of last status change */
    
    int hidden; /**< whether the node will be hidden, see IsoHideNodeFlag */   

    IsoDir *parent; /**< parent node, NULL for root */

    /*
     * Pointer to the linked list of children in a dir.
     */
    IsoNode *next;
};

struct Iso_Dir
{
    IsoNode node;

    size_t nchildren; /**< The number of children of this directory. */
    IsoNode *children; /**< list of children. ptr to first child */
};

struct Iso_File
{
    IsoNode node;
    
    /**
     * Location of a file extent in a ms disc, 0 for newly added file
     */
    uint32_t msblock;

	/** 
	 * It sorts the order in which the file data is written to the CD image.
	 * Higher weighting files are written at the beginning of image 
	 */
    int sort_weight;
    IsoStream *stream;
};

struct Iso_Symlink
{
    IsoNode node;
    
    char *dest;
};

struct Iso_Special
{
    IsoNode node;
    dev_t dev;
};

/**
 * An iterator for directory children.
 */
struct Iso_Dir_Iter
{
    const IsoDir *dir;
    IsoNode *pos;
};

int iso_node_new_root(IsoDir **root);

#endif /*LIBISO_NODE_H_*/
