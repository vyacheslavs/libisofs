/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_LIBISOFS_H_
#define LIBISO_LIBISOFS_H_

#include <sys/stat.h>
 
typedef struct Iso_Node IsoNode;
typedef struct Iso_Dir IsoDir;
typedef struct Iso_Symlink IsoSymlink;

/**
 * Increments the reference counting of the given node.
 */
void iso_node_ref(IsoNode *node);

/**
 * Decrements the reference couting of the given node.
 * If it reach 0, the node is free, and, if the node is a directory,
 * its children will be unref() too.
 */
void iso_node_unref(IsoNode *node);

/**
 * Set the name of a node.
 * 
 * @param name  The name in UTF-8 encoding
 */
void iso_node_set_name(IsoNode *node, const char *name);

/**
 * Get the name of a node (in UTF-8).
 * The returned string belongs to the node and should not be modified nor
 * freed. Use strdup if you really need your own copy.
 */
const char *iso_node_get_name(const IsoNode *node);

/**
 * Set the permissions for the node. This attribute is only useful when 
 * Rock Ridge extensions are enabled.
 * 
 * @param mode 
 *     bitmask with the permissions of the node, as specified in 'man 2 stat'.
 *     The file type bitfields will be ignored, only file permissions will be
 *     modified.
 */
void iso_node_set_permissions(IsoNode *node, mode_t mode);

/** 
 * Get the permissions for the node 
 */
mode_t iso_node_get_permissions(const IsoNode *node);

/** 
 * Get the mode of the node, both permissions and file type, as specified in
 * 'man 2 stat'.
 */
mode_t iso_node_get_mode(const IsoNode *node);

/**
 * Set the user id for the node. This attribute is only useful when 
 * Rock Ridge extensions are enabled.
 */
void iso_node_set_uid(IsoNode *node, uid_t uid);

/**
 * Get the user id of the node.
 */
uid_t iso_node_get_uid(const IsoNode *node);

/**
 * Set the group id for the node. This attribute is only useful when 
 * Rock Ridge extensions are enabled.
 */
void iso_node_set_gid(IsoNode *node, gid_t gid);

/**
 * Get the group id of the node.
 */
gid_t iso_node_get_gid(const IsoNode *node);

#endif /*LIBISO_LIBISOFS_H_*/
