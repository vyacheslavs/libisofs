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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * The type of an IsoNode.
 */
enum IsoNodeType {
    LIBISO_DIR,
    LIBISO_FILE,
    LIBISO_SYMLINK,
    LIBISO_SPECIAL,
    LIBISO_BOOT
};

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

    struct IsoDir *parent; /**< parent node, NULL for root */

    /*
     * Pointers to the doubled linked list of children in a dir.
     * It is a circular list where the last child points to the first
     * and viceversa.
     */
    IsoNode *prev;
    IsoNode *next;
};

struct Iso_Dir
{
    IsoNode node;

    size_t nchildren; /**< The number of children of this directory. */
    struct IsoNode *children; /**< list of children. ptr to first child */
};

struct Iso_File
{
    IsoNode node;

	/** 
	 * It sorts the order in which the file data is written to the CD image.
	 * Higher weighting filesare written at the beginning of image 
	 */
    int sort_weight;
    
};


#endif /*LIBISO_NODE_H_*/
