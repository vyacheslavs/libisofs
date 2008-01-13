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

/* #define LIBISO_EXTENDED_INFORMATION */
#ifdef LIBISO_EXTENDED_INFORMATION

/**
 * The extended information is a way to attach additional information to each
 * IsoNode. External applications may want to use this extension system to 
 * store application speficic information related to each node. On the other
 * side, libisofs may make use of this struct to attach information to nodes in
 * some particular, uncommon, cases, without incrementing the size of the
 * IsoNode struct.
 * 
 * It is implemented like a chained list.
 */
typedef struct iso_extended_info IsoExtendedInfo;

struct iso_extended_info {
    /**
     * Next struct in the chain. NULL if it is the last item
     */
    IsoExtendedInfo *next;
    
    /**
     * Function to handle this particular extended information. The function
     * pointer acts as an identifier for the type of the information. Structs
     * with same information type must use the same function.
     * 
     * @param data
     *     Attached data
     * @param flag
     *     What to do with the data. At this time the following values are 
     *     defined:
     *      -> 1 the data must be freed
     * @return
     *     1
     */
    int (*process)(void *data, int flag);
    
    /**
     * Pointer to information specific data.
     */
    void *data;
};

#endif

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

    /** Type of the IsoNode, do not confuse with mode */
    enum IsoNodeType type;

    char *name; /**< Real name, in default charset */

    mode_t mode; /**< protection */
    uid_t uid; /**< user ID of owner */
    gid_t gid; /**< group ID of owner */

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

#ifdef LIBISO_EXTENDED_INFORMATION
    /**
     * Extended information for the node.
     */
    IsoExtendedInfo *xinfo;
#endif
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

/**
 * Check if a given name is valid for an iso node.
 * 
 * @return
 *     1 if yes, 0 if not
 */
int iso_node_is_valid_name(const char *name);

/**
 * Check if a given path is valid for the destination of a link.
 * 
 * @return
 *     1 if yes, 0 if not
 */
int iso_node_is_valid_link_dest(const char *dest);

/**
 * Find the position where to insert a node
 * 
 * @param dir
 *      A valid dir. It can't be NULL
 * @param name
 *      The node name to search for. It can't be NULL
 * @param pos
 *      Will be filled with the position where to insert. It can't be NULL
 */
void iso_dir_find(IsoDir *dir, const char *name, IsoNode ***pos);

/**
 * Check if a node with the given name exists in a dir.
 * 
 * @param dir
 *      A valid dir. It can't be NULL
 * @param name
 *      The node name to search for. It can't be NULL
 * @param pos
 *      If not NULL, will be filled with the position where to insert. If the
 *      node exists, (**pos) will refer to the given node.
 * @return
 *      1 if node exists, 0 if not
 */
int iso_dir_exists(IsoDir *dir, const char *name, IsoNode ***pos);

/**
 * Inserts a given node in a dir, at the specified position.
 * 
 * @param dir
 *     Dir where to insert. It can't be NULL
 * @param node
 *     The node to insert. It can't be NULL
 * @param pos
 *     Position where the node will be inserted. It is a pointer previously
 *     obtained with a call to iso_dir_exists() or iso_dir_find(). 
 *     It can't be NULL.
 * @param replace 
 *     Whether to replace an old node with the same name with the new node.
 * @return
 *     If success, number of children in dir. < 0 on error  
 */
int iso_dir_insert(IsoDir *dir, IsoNode *node, IsoNode **pos, 
                   enum iso_replace_mode replace);

#endif /*LIBISO_NODE_H_*/
