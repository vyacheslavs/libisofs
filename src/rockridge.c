/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2007 Mario Danic
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "rockridge.h"
#include "node.h"
#include "ecma119_tree.h"
#include "error.h"

#include <string.h>

/**
 * Compute the length needed for write all RR and SUSP entries for a given
 * node.
 * 
 * @param type
 *      0 normal entry, 1 "." entry for that node (it is a dir), 2 ".."
 *      for that node (i.e., it will refer to the parent)
 * @param space
 *      Available space in the System Use Area for the directory record.
 * @param ce
 *      Will be filled with the space needed in a CE
 * @return
 *      The size needed for the RR entries in the System Use Area
 */
size_t rrip_calc_len(Ecma119Image *t, Ecma119Node *n, int type,
                     size_t space, size_t *ce)
{
    size_t su_size;
    
    /* space min is 255 - 33 - 37 = 185 */
    
    /* PX and TF, we are sure they always fit in SUA */
    su_size = 44 + 26;
    
    if (n->type == ECMA119_DIR) {
        if (n->info.dir.real_parent != NULL) {
            /* it is a reallocated entry */
            if (type == 2) {
                /* we need to add a PL entry */
                su_size += 12;
            } else if (type == 0) {
                /* we need to add a RE entry */
                su_size += 4;
            }
        }
    } else if (n->type == ECMA119_SPECIAL) {
        if (S_ISBLK(n->node->mode) || S_ISCHR(n->node->mode)) {
            /* block or char device, we need a PN entry */
            su_size += 20;
        } 
    } else if (n->type == ECMA119_PLACEHOLDER) {
        /* we need the CL entry */
        su_size += 12;
    }
    
    if (type == 0) {
        size_t namelen = strlen(n->node->name);
        
        /* NM entry */
        if (su_size + 5 + namelen <= space) {
            /* ok, it fits in System Use Area */
            su_size += 5 + namelen;
        } else {
            /* the NM will be divided in a CE */
            namelen = namelen - (space - su_size - 5 - 28);
            *ce = 5 + namelen;
            su_size = space;
        }
        if (n->type == ECMA119_SYMLINK) {
            /* 
             * for symlinks, we also need to write the SL
             */
            char *cur, *prev;
            size_t sl_len = 5;
            int cew = (*ce != 0); /* are we writing to CE? */
            
            prev = ((IsoSymlink*)n->node)->dest;
            cur = strchr(prev, '/');
            while (1) {
                size_t clen;
                if (cur) {
                    clen = cur - prev;
                } else {
                    /* last component */
                    clen = strlen(prev);
                }
                
                if (clen == 1 && prev[0] == '.') {
                    clen = 0;
                } else if (clen == 2 && prev[0] == '.' && prev[1] == '.') {
                    clen = 0;
                }
                
                /* flags and len for each component record (RRIP, 4.1.3.1) */
                clen += 2;
                
                if (!cew) {
                    /* we are still writing to the SUA */
                    if (su_size + sl_len + clen > space) {
                        /* 
                         * ok, we need a Continuation Area anyway
                         * TODO this can be handled better, but for now SL
                         * will be completelly moved into the CA
                         */
                        if (su_size + 28 <= space) {
                            /* the CE entry fills without reducing NM */
                            su_size += 28;
                            cew = 1;
                        } else {
                            /* we need to reduce NM */
                            *ce = (28 - (space - su_size)) + 5;
                            su_size = space;
                            cew = 1;
                        }
                    } 
                } else {
                    if (sl_len + clen > 255) {
                        /* we need an addition SL entry */
                        if (clen > 250) {
                            /* 
                             * case 1, component too large to fit in a 
                             * single SL entry. Thus, the component need
                             * to be divided anyway.
                             * Note than clen can be up to 255 + 2 = 257.
                             * 
                             * First, we check how many bytes fit in current
                             * SL field 
                             */
                            int fit = 255 - sl_len - 2;
                            if (clen - 250 <= fit) {
                                /* 
                                 * the component can be divided between this
                                 * and another SL entry
                                 */
                                *ce += 255; /* this SL, full */
                                sl_len = 5 + (clen - fit);
                            } else {
                                /*
                                 * the component will need a 2rd SL entry in
                                 * any case, so we prefer to don't write 
                                 * anything in this SL
                                 */
                                *ce += sl_len + 255;
                                sl_len = 5 + (clen - 250);
                            }
                        } else {
                            /* case 2, create a new SL entry */
                            *ce += sl_len;
                            sl_len = 5 + clen;
                        }
                    } else {
                        sl_len += clen;
                    }
                }
                
                if (!cur || cur[1] == '\0') {
                    /* cur[1] can be \0 if dest ends with '/' */
                    break;
                }
                prev = cur + 1;
                cur = strchr(prev, '/');
            }
            
            /* and finally write the pending SL field */
            if (!cew) {
                /* the whole SL fits into the SUA */
                su_size += sl_len;
            } else {
                *ce += sl_len;
            }
            
        }
    } else {
        
        /* "." or ".." entry */
        su_size += 5; /* NM field */
        if (type == 1 && n->parent == NULL) {
            /* 
             * "." for root directory 
             * we need to write SP and ER entries. The first fits in SUA,
             * ER needs a Continuation Area, thus we also need a CE entry
             */
            su_size += 7 + 28; /* SP + CE */
            *ce = 182; /* ER */
        }
    }
    
    /*
     * The System Use field inside the directory record must be padded if
     * it is an odd number (ECMA-119, 9.1.13)
     */
    su_size += (su_size % 2);
    return su_size;
}




