/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_LIBISOFS_H_
#define LIBISO_LIBISOFS_H_

typedef struct Iso_Tree_Node IsoTreeNode;

/**
 * Increments the reference counting of the given node.
 */
void iso_node_ref(IsoTreeNode *node);


#endif /*LIBISO_LIBISOFS_H_*/
