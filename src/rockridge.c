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
#include "writer.h"

#include <string.h>

static
int susp_append(Ecma119Image *t, struct susp_info *susp, uint8_t *data)
{
    susp->n_susp_fields++;
    susp->susp_fields = realloc(susp->susp_fields,
                    sizeof(void*) * susp->n_susp_fields);
    if (susp->susp_fields == NULL) {
        return ISO_MEM_ERROR;
    }
    susp->susp_fields[susp->n_susp_fields - 1] = data;
    susp->suf_len += data[2];
    return ISO_SUCCESS;
}

static
int susp_append_ce(Ecma119Image *t, struct susp_info *susp, uint8_t *data)
{
    susp->n_ce_susp_fields++;
    susp->ce_susp_fields = realloc(susp->ce_susp_fields,
                    sizeof(void*) * susp->n_ce_susp_fields);
    if (susp->ce_susp_fields == NULL) {
        return ISO_MEM_ERROR;
    }
    susp->ce_susp_fields[susp->n_ce_susp_fields - 1] = data;
    susp->ce_len += data[2];
    return ISO_SUCCESS;
}

/**
 * Add a PX System Use Entry. The PX System Use Entry is used to add POSIX 
 * file attributes, such as access permissions or user and group id, to a 
 * ECMA 119 directory record. (RRIP, 4.1.1)
 */
static
int rrip_add_PX(Ecma119Image *t, Ecma119Node *n, struct susp_info *susp)
{
    uint8_t *PX = malloc(44);
    if (PX == NULL) {
        return ISO_MEM_ERROR;
    }

    PX[0] = 'P';
    PX[1] = 'X';
    PX[2] = 44;
    PX[3] = 1;
    iso_bb(&PX[4], n->node->mode, 4);
    iso_bb(&PX[12], n->nlink, 4);
    iso_bb(&PX[20], n->node->uid, 4);
    iso_bb(&PX[28], n->node->gid, 4);
    iso_bb(&PX[36], n->ino, 4);
    
    return susp_append(t, susp, PX);
}

/**
 * Add to the given tree node a TF System Use Entry, used to record some
 * time stamps related to the file (RRIP, 4.1.6).
 */
static
int rrip_add_TF(Ecma119Image *t, Ecma119Node *n, struct susp_info *susp)
{
    uint8_t *TF = malloc(5 + 3 * 7);
    if (TF == NULL) {
        return ISO_MEM_ERROR;
    }

    TF[0] = 'T';
    TF[1] = 'F';
    TF[2] = 5 + 3 * 7;
    TF[3] = 1;
    TF[4] = (1 << 1) | (1 << 2) | (1 << 3);
    iso_datetime_7(&TF[5], n->node->mtime);
    iso_datetime_7(&TF[12], n->node->atime);
    iso_datetime_7(&TF[19], n->node->ctime);
    return susp_append(t, susp, TF);
}

/**
 * Add a PL System Use Entry, used to record the location of the original 
 * parent directory of a directory which has been relocated.
 * 
 * This is special because it doesn't modify the susp fields of the directory
 * that gets passed to it; it modifies the susp fields of the ".." entry in
 * that directory.
 * 
 * See RRIP, 4.1.5.2 for more details.
 */
static
int rrip_add_PL(Ecma119Image *t, Ecma119Node *n, struct susp_info *susp)
{
    uint8_t *PL;
    
    if (n->type != ECMA119_DIR || n->info.dir.real_parent == NULL) {
        /* should never occur */
        return ISO_ERROR;
    }
    
    PL = malloc(12);
    if (PL == NULL) {
        return ISO_MEM_ERROR;
    }

    PL[0] = 'P';
    PL[1] = 'L';
    PL[2] = 12;
    PL[3] = 1;
    
    /* write the location of the real parent, already computed */
    iso_bb(&PL[4], n->info.dir.real_parent->info.dir.block, 4);
    return susp_append(t, susp, PL);
}

/**
 * Add a RE System Use Entry to the given tree node. The purpose of the 
 * this System Use Entry is to indicate to an RRIP-compliant receiving
 * system that the Directory Record in which an "RE" System Use Entry is 
 * recorded has been relocated from another position in the original 
 * Directory Hierarchy.
 * 
 * See RRIP, 4.1.5.3 for more details.
 */
static
int rrip_add_RE(Ecma119Image *t, Ecma119Node *n, struct susp_info *susp)
{
    uint8_t *RE = malloc(4);
    if (RE == NULL) {
        return ISO_MEM_ERROR;
    }

    RE[0] = 'R';
    RE[1] = 'E';
    RE[2] = 4;
    RE[3] = 1;
    return susp_append(t, susp, RE);
}

/**
 * Add a PN System Use Entry to the given tree node. 
 * The PN System Use Entry is used to store the device number, and it's
 * mandatory if the tree node corresponds to a character or block device.
 * 
 * See RRIP, 4.1.2 for more details.
 */
static
int rrip_add_PN(Ecma119Image *t, Ecma119Node *n, struct susp_info *susp)
{
    IsoSpecial *node;
    uint8_t *PN;
    
    node = (IsoSpecial*)n->node;
    if (node->node.type != LIBISO_SPECIAL) {
        /* should never occur */
        return ISO_ERROR;
    }
    
    PN = malloc(20);
    if (PN == NULL) {
        return ISO_MEM_ERROR;
    }

    PN[0] = 'P';
    PN[1] = 'N';
    PN[2] = 20;
    PN[3] = 1;
    iso_bb(&PN[4], node->dev >> 32, 4);
    iso_bb(&PN[12], node->dev & 0xffffffff, 4);
    return susp_append(t, susp, PN);
}

/**
 * Add to the given tree node a CL System Use Entry, that is used to record 
 * the new location of a directory which has been relocated.
 * 
 * See RRIP, 4.1.5.1 for more details.
 */
static
int rrip_add_CL(Ecma119Image *t, Ecma119Node *n, struct susp_info *susp)
{
    uint8_t *CL;
    if (n->type != ECMA119_PLACEHOLDER) {
        /* should never occur */
        return ISO_ERROR;
    }
    CL = malloc(12);
    if (CL == NULL) {
        return ISO_MEM_ERROR;
    }

    CL[0] = 'C';
    CL[1] = 'L';
    CL[2] = 12;
    CL[3] = 1;
    iso_bb(&CL[4], n->info.real_me->info.dir.block, 4);
    return susp_append(t, susp, CL);
}

/**
 * Add a NM System Use Entry to the given tree node. The purpose of this
 * System Use Entry is to store the content of an Alternate Name to support 
 * POSIX-style or other names. 
 * 
 * See RRIP, 4.1.4 for more details.
 * 
 * @param size
 *     Length of the name to be included into the NM
 * @param flags
 * @param ce
 *     Whether to add or not to CE
 */
static
int rrip_add_NM(Ecma119Image *t, struct susp_info *susp,
                char *name, int size, int flags, int ce)
{
    uint8_t *NM = malloc(size + 5);
    if (NM == NULL) {
        return ISO_MEM_ERROR;
    }

    NM[0] = 'N';
    NM[1] = 'M';
    NM[2] = size + 5;
    NM[3] = 1;
    NM[4] = flags;
    if (size) {
        memcpy(&NM[5], name, size);
    }
    if (ce) {
        return susp_append_ce(t, susp, NM);
    } else {
        return susp_append(t, susp, NM);
    }
}

/**
 * Add a new SL component (RRIP, 4.1.3.1) to a list of components.
 * 
 * @param n
 *     Number of components. It will be updated.
 * @param compos
 *     Pointer to the list of components.
 * @param s
 *     The component content
 * @param size
 *     Size of the component content
 * @param fl
 *     Flags
 * @return
 *     1 on success, < 0 on error
 */
static 
int rrip_SL_append_comp(size_t *n, uint8_t ***comps,
                         char *s, int size, char fl)
{
    uint8_t *comp = malloc(size + 2);
    if (comp == NULL) {
        return ISO_MEM_ERROR;
    }

    (*n)++;
    comp[0] = fl;
    comp[1] = size;
    *comps = realloc(*comps, (*n) * sizeof(void*));
    if (*comps == NULL) {
        free(comp);
        return ISO_MEM_ERROR;
    }
    (*comps)[(*n) - 1] = comp;

    if (size) {
        memcpy(&comp[2], s, size);
    }
    return ISO_SUCCESS;
}

/**
 * Add a SL System Use Entry to the given tree node. This is used to store 
 * the content of a symbolic link, and is mandatory if the tree node
 * indicates a symbolic link (RRIP, 4.1.3).
 * 
 * @param comp
 *     Components of the SL System Use Entry. If they don't fit in a single
 *     SL, more than one SL will be added.
 * @param n
 *     Number of components in comp
 * @param ce
 *     Whether to add to a continuation area or system use field.
 */
static
int rrip_add_SL(Ecma119Image *t, struct susp_info *susp,
                uint8_t **comp, size_t n, int ce)
{
    int ret, i, j;

    int total_comp_len = 0;
    size_t pos, written = 0;

    uint8_t *SL;

    for (i = 0; i < n; i++) {
        
        total_comp_len += comp[i][1] + 2;
        if (total_comp_len > 250) {
            /* we need a new SL entry */
            total_comp_len -= comp[i][1] + 2;
            SL = malloc(total_comp_len + 5);
            if (SL == NULL) {
                return ISO_MEM_ERROR;
            }
            
            SL[0] = 'S';
            SL[1] = 'L';
            SL[2] = total_comp_len + 5;
            SL[3] = 1;
            SL[4] = 1;  /* CONTINUE */
            pos = 5;
            for (j = written; j < i; j++) {
                memcpy(&SL[pos], comp[j], comp[j][1] + 2);
                pos += comp[j][1] + 2;
            }
            
            /*
             * In this case we are sure we're writting to CE. Check for
             * debug purposes
             */
            if (ce == 0) {
                return ISO_ERROR; /* unexpected */
            }
            ret = susp_append_ce(t, susp, SL);
            if (ret < 0) {
                return ret;
            }
            written = i;
            total_comp_len = comp[i][1] + 2;
        }
    }
    
    SL = malloc(total_comp_len + 5);
    if (SL == NULL) {
        return ISO_MEM_ERROR;
    }
    
    SL[0] = 'S';
    SL[1] = 'L';
    SL[2] = total_comp_len + 5;
    SL[3] = 1;
    SL[4] = 0;
    pos = 5;

    for (j = written; j < n; j++) {
        memcpy(&SL[pos], comp[j], comp[j][1] + 2);
        pos += comp[j][1] + 2;
    }
    if (ce) {
        ret = susp_append_ce(t, susp, SL);
    } else {
        ret = susp_append(t, susp, SL);
    }
    return ret;
}

/**
 * Add a SUSP "ER" System Use Entry to identify the Rock Ridge specification.
 * 
 * The "ER" System Use Entry is used to uniquely identify a specification
 * compliant with SUSP. This method adds to the given tree node "." entry
 * the "ER" corresponding to the RR protocol.
 * 
 * See SUSP, 5.5 and RRIP, 4.3 for more details.
 */
static
int rrip_add_ER(Ecma119Image *t, struct susp_info *susp)
{
    unsigned char *ER = malloc(182);
    if (ER == NULL) {
        return ISO_MEM_ERROR;
    }

    ER[0] = 'E';
    ER[1] = 'R';
    ER[2] = 182;
    ER[3] = 1;
    ER[4] = 9;
    ER[5] = 72;
    ER[6] = 93;
    ER[7] = 1;
    memcpy(&ER[8], "IEEE_1282", 9);
    memcpy(&ER[17], "THE IEEE 1282 PROTOCOL PROVIDES SUPPORT FOR POSIX "
           "FILE SYSTEM SEMANTICS.", 72);
    memcpy(&ER[89], "PLEASE CONTACT THE IEEE STANDARDS DEPARTMENT, "
           "PISCATAWAY, NJ, USA FOR THE 1282 SPECIFICATION.", 93);
           
    /** This always goes to continuation area */
    return susp_append_ce(t, susp, ER);
}

/**
 * Add a CE System Use Entry to the given tree node. A "CE" is used to add
 * a continuation area, where additional System Use Entry can be written.
 * (SUSP, 5.1).
 */
static
int susp_add_CE(Ecma119Image *t, size_t ce_len, struct susp_info *susp)
{
    uint8_t *CE = malloc(28);
    if (CE == NULL) {
        return ISO_MEM_ERROR;
    }

    CE[0] = 'C';
    CE[1] = 'E';
    CE[2] = 28;
    CE[3] = 1;
    iso_bb(&CE[4], susp->ce_block, 4);
    iso_bb(&CE[12], susp->ce_len, 4);
    iso_bb(&CE[20], ce_len, 4);
    
    return susp_append(t, susp, CE);
}

/**
 * Add a SP System Use Entry. The SP provide an identifier that the SUSP is 
 * used within the volume. The SP shall be recorded in the "." entry of the 
 * root directory. See SUSP, 5.3 for more details.
 */
static
int susp_add_SP(Ecma119Image *t, struct susp_info *susp)
{
    unsigned char *SP = malloc(7);
    if (SP == NULL) {
        return ISO_MEM_ERROR;
    }

    SP[0] = 'S';
    SP[1] = 'P';
    SP[2] = (char)7;
    SP[3] = (char)1;
    SP[4] = 0xbe;
    SP[5] = 0xef;
    SP[6] = 0;
    return susp_append(t, susp, SP);
}

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
    
    /* space min is 255 - 33 - 37 = 185
     * At the same time, it is always an odd number, but we need to pad it
     * propertly to ensure the length of a directory record is a even number
     * (ECMA-119, 9.1.13). Thus, in fact the real space is always space - 1
     */
    space--;
    *ce = 0;
    
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
                        } else {
                            /* we need to reduce NM */
                            *ce = (28 - (space - su_size)) + 5;
                            su_size = space;
                        }
                        cew = 1;
                    } else {
                        sl_len += clen;
                    }
                } 
                if (cew) {
                    if (sl_len + clen > 255) {
                        /* we need an additional SL entry */
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
                                sl_len = 5 + (clen - 250) + 2;
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

/**
 * Free all info in a struct susp_info.
 */
static
void susp_info_free(struct susp_info* susp)
{
    size_t i;
    
    for (i = 0; i < susp->n_susp_fields; ++i) {
        free(susp->susp_fields[i]);
    }
    free(susp->susp_fields);
    
    for (i = 0; i < susp->n_ce_susp_fields; ++i) {
        free(susp->ce_susp_fields[i]);
    }
    free(susp->ce_susp_fields);
}

/**
 * Fill a struct susp_info with the RR/SUSP entries needed for a given
 * node.
 * 
 * @param type
 *      0 normal entry, 1 "." entry for that node (it is a dir), 2 ".."
 *      for that node (i.e., it will refer to the parent)
 * @param space
 *      Available space in the System Use Area for the directory record.
 * @param info
 *      Pointer to the struct susp_info where the entries will be stored.
 *      If some entries need to go to a Continuation Area, they will be added
 *      to the existing ce_susp_fields, and ce_len will be incremented
 *      propertly. Please ensure ce_block is initialized propertly.
 * @return
 *      1 success, < 0 error
 */
int rrip_get_susp_fields(Ecma119Image *t, Ecma119Node *n, int type, 
                         size_t space, struct susp_info *info)
{
    int ret;
    size_t i;
    Ecma119Node *node;
    
    if (t == NULL || n == NULL || info == NULL) {
        return ISO_NULL_POINTER;
    }
    if (type < 0 || type > 2 || space < 185) {
        /* space min is 255 - 33 - 37 = 185 */
        return ISO_WRONG_ARG_VALUE;
    }
    
    if (type == 2 && n->parent != NULL) {
        node = n->parent;
    } else {
        node = n;
    }
    
    /* space min is 255 - 33 - 37 = 185
     * At the same time, it is always an odd number, but we need to pad it
     * propertly to ensure the length of a directory record is a even number
     * (ECMA-119, 9.1.13). Thus, in fact the real space is always space - 1
     */
    space--;
    
    /* 
     * SP must be the first entry for the "." record of the root directory
     * (SUSP, 5.3)
     */
    if (type == 1 && n->parent == NULL) {
        ret = susp_add_SP(t, info);
        if (ret < 0) {
            goto add_susp_cleanup;
        }
    }
    
    /* PX and TF, we are sure they always fit in SUA */
    ret = rrip_add_PX(t, node, info);
    if (ret < 0) {
        goto add_susp_cleanup;
    }
    ret = rrip_add_TF(t, node, info);
    if (ret < 0) {
        goto add_susp_cleanup;
    }
    
    if (n->type == ECMA119_DIR) {
        if (n->info.dir.real_parent != NULL) {
            /* it is a reallocated entry */
            if (type == 2) {
                /* 
                 * we need to add a PL entry
                 * Note that we pass "n" as parameter, not "node" 
                 */
                ret = rrip_add_PL(t, n, info);
                if (ret < 0) {
                    goto add_susp_cleanup;
                }
            } else if (type == 0) {
                /* we need to add a RE entry */
                ret = rrip_add_RE(t, node, info);
                if (ret < 0) {
                    goto add_susp_cleanup;
                }
            }
        }
    } else if (n->type == ECMA119_SPECIAL) {
        if (S_ISBLK(n->node->mode) || S_ISCHR(n->node->mode)) {
            /* block or char device, we need a PN entry */
            ret = rrip_add_PN(t, node, info);
            if (ret < 0) {
                goto add_susp_cleanup;
            }
        } 
    } else if (n->type == ECMA119_PLACEHOLDER) {
        /* we need the CL entry */
        ret = rrip_add_CL(t, node, info);
        if (ret < 0) {
            goto add_susp_cleanup;
        }
    }
    
    if (type == 0) {
        char *name;
        size_t sua_free; /* free space in the SUA */
        int nm_type = 0; /* 0 whole entry in SUA, 1 part in CE */
        size_t ce_len = 0; /* len of the CE */
        size_t namelen;
        
        /* this two are only defined for symlinks */
        uint8_t **comps = NULL; /* components of the SL field */
        size_t n_comp = 0; /* number of components */
        
        // TODO handle output charset
        name = n->node->name;
        namelen = strlen(name);
        
        sua_free = space - info->suf_len;
        
        /* NM entry */
        if (5 + namelen <= sua_free) {
            /* ok, it fits in System Use Area */
            sua_free -= (5 + namelen);
            nm_type = 0;
        } else {
            /* the NM will be divided in a CE */
            nm_type = 1;
            namelen = namelen - (sua_free - 5 - 28);
            ce_len = 5 + namelen;
            sua_free = 0;
        }
        if (n->type == ECMA119_SYMLINK) {
            /* 
             * for symlinks, we also need to write the SL
             */
            char *cur, *prev;
            size_t sl_len = 5;
            int cew = (nm_type == 1); /* are we writing to CE? */
            
            prev = ((IsoSymlink*)n->node)->dest;
            cur = strchr(prev, '/');
            while (1) {
                size_t clen;
                char cflag = 0; /* component flag (RRIP, 4.1.3.1) */
                if (cur) {
                    clen = cur - prev;
                } else {
                    /* last component */
                    clen = strlen(prev);
                }
                
                if (clen == 0) {
                    /* this refers to the roor directory, '/' */
                    cflag = 1 << 3;
                } if (clen == 1 && prev[0] == '.') {
                    clen = 0;
                    cflag = 1 << 1;
                } else if (clen == 2 && prev[0] == '.' && prev[1] == '.') {
                    clen = 0;
                    cflag = 1 << 2;
                }
                
                /* flags and len for each component record (RRIP, 4.1.3.1) */
                clen += 2;
                
                if (!cew) {
                    /* we are still writing to the SUA */
                    if (sl_len + clen > sua_free) {
                        /* 
                         * ok, we need a Continuation Area anyway
                         * TODO this can be handled better, but for now SL
                         * will be completelly moved into the CA
                         */
                        if (28 <= sua_free) {
                            /* the CE entry fills without reducing NM */
                            sua_free -= 28;
                            cew = 1;
                        } else {
                            /* we need to reduce NM */
                            nm_type = 1;
                            ce_len = (28 - sua_free) + 5;
                            sua_free = 0;
                            cew = 1;
                        }
                    } else {
                        /* add the component */
                        ret = rrip_SL_append_comp(&n_comp, &comps, prev, 
                                                  clen - 2, cflag);
                        if (ret < 0) {
                            goto add_susp_cleanup;
                        }
                        sl_len += clen;
                    }
                } 
                if (cew) {
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
                                ret = rrip_SL_append_comp(&n_comp, &comps, 
                                                          prev, fit, 0x01);
                                if (ret < 0) {
                                    goto add_susp_cleanup;
                                }
                                /* 
                                 * and another component, that will go in 
                                 * other SL entry
                                 */
                                ret = rrip_SL_append_comp(&n_comp, &comps, 
                                                          prev + fit, 
                                                          clen - fit - 2, 
                                                          0);
                                if (ret < 0) {
                                    goto add_susp_cleanup;
                                }
                                ce_len += 255; /* this SL, full */
                                sl_len = 5 + (clen - fit);
                            } else {
                                /*
                                 * the component will need a 2rd SL entry in
                                 * any case, so we prefer to don't write 
                                 * anything in this SL
                                 */
                                ret = rrip_SL_append_comp(&n_comp, &comps, 
                                                          prev, 248, 0x01);
                                if (ret < 0) {
                                    goto add_susp_cleanup;
                                }
                                ret = rrip_SL_append_comp(&n_comp, &comps, 
                                                          prev + 248, 
                                                          strlen(prev + 248),
                                                          0x00);
                                if (ret < 0) {
                                    goto add_susp_cleanup;
                                }
                                ce_len += sl_len + 255;
                                sl_len = 5 + (clen - 250) + 2;
                            }
                        } else {
                            /* case 2, create a new SL entry */
                            ret = rrip_SL_append_comp(&n_comp, &comps, prev, 
                                                      clen - 2, cflag);
                            if (ret < 0) {
                                goto add_susp_cleanup;
                            }
                            ce_len += sl_len;
                            sl_len = 5 + clen;
                        }
                    } else {
                        /* the component fit in the SL entry */
                        ret = rrip_SL_append_comp(&n_comp, &comps, prev, 
                                                  clen - 2, cflag);
                        if (ret < 0) {
                            goto add_susp_cleanup;
                        }
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
            
            if (cew) {
                ce_len += sl_len;
            }
        }
            
        /*
         * We we reach here:
         * - We know if NM fill in the SUA (nm_type == 0)
         * - If SL needs an to be written in CE (ce_len > 0)
         * - The components for SL entry (or entries)
         */
        
        if (nm_type == 0) {
            /* the full NM fills in SUA */
            ret = rrip_add_NM(t, info, name, strlen(name), 0, 0);
            if (ret < 0) {
                goto add_susp_cleanup;
            }
        } else {
            /* 
             * Write the NM part that fits in SUA...  Note that CE
             * entry and NM in the continuation area is added below 
             */
            size_t len = space - info->suf_len - 28 - 5;
            ret = rrip_add_NM(t, info, name, len, 1, 0);
            if (ret < 0) {
                goto add_susp_cleanup;
            }
            name += len;
        }
        
        if (ce_len > 0) {
            /* Add the CE entry */
            ret = susp_add_CE(t, ce_len, info);
            if (ret < 0) {
                goto add_susp_cleanup;
            }
        }

        if (nm_type == 1) {
            /* 
             * ..and the part that goes to continuation area.
             */
            ret = rrip_add_NM(t, info, name, strlen(name), 0, 1);
            if (ret < 0) {
                goto add_susp_cleanup;
            }
        }

        if (n->type == ECMA119_SYMLINK) {
            
            /* add the SL entry (or entries) */
            ret = rrip_add_SL(t, info, comps, n_comp, (ce_len > 0));
        
            /* free the components */
            for (i = 0; i < n_comp; i++) {
                free(comps[i]);
            }
            free(comps);
            
            if (ret < 0) {
                goto add_susp_cleanup;
            }
        }
        
        
    } else {
        
        /* "." or ".." entry */
        
        /* write the NM entry */
        ret = rrip_add_NM(t, info, NULL, 0, 1 << type, 0);
        if (ret < 0) {
            goto add_susp_cleanup;
        }
        if (type == 1 && n->parent == NULL) {
            /* 
             * "." for root directory 
             * we need to write SP and ER entries. The first fits in SUA,
             * ER needs a Continuation Area, thus we also need a CE entry.
             * Note that SP entry was already added above
             */
            ret = susp_add_CE(t, 182, info); /* 182 is ER length */
            if (ret < 0) {
                goto add_susp_cleanup;
            }
            ret = rrip_add_ER(t, info);
            if (ret < 0) {
                goto add_susp_cleanup;
            }
        }
    }
    
    /*
     * The System Use field inside the directory record must be padded if
     * it is an odd number (ECMA-119, 9.1.13)
     */
    info->suf_len += (info->suf_len % 2);
    
    return ISO_SUCCESS;
    
add_susp_cleanup:;
    susp_info_free(info);
    return ret;
}

/**
 * Write the given SUSP fields into buf. Note that Continuation Area
 * fields are not written.
 * If info does not contain any SUSP entry this function just return. 
 * After written, the info susp_fields array will be freed, and the counters
 * updated propertly.
 */
void rrip_write_susp_fields(Ecma119Image *t, struct susp_info *info, 
                            uint8_t *buf)
{
    size_t i;
    size_t pos = 0;
    
    if (info->n_susp_fields == 0) {
        return;
    }

    for (i = 0; i < info->n_susp_fields; i++) {
        memcpy(buf + pos, info->susp_fields[i], info->susp_fields[i][2]);
        pos += info->susp_fields[i][2];
    }
    
    /* free susp_fields */
    for (i = 0; i < info->n_susp_fields; ++i) {
        free(info->susp_fields[i]);
    }
    free(info->susp_fields);
    info->susp_fields = NULL;
    info->n_susp_fields = 0;
    info->suf_len = 0;
}

/**
 * Write the Continuation Area entries for the given struct susp_info, using
 * the iso_write() function.
 * After written, the ce_susp_fields array will be freed.
 */
int rrip_write_ce_fields(Ecma119Image *t, struct susp_info *info)
{
    size_t i;
    uint8_t padding[BLOCK_SIZE];
    int ret = ISO_SUCCESS;
    
    if (info->n_ce_susp_fields == 0) {
        return ret;
    }

    for (i = 0; i < info->n_ce_susp_fields; i++) {
        ret = iso_write(t, info->ce_susp_fields[i], 
                        info->ce_susp_fields[i][2]);
        if (ret < 0) {
            goto write_ce_field_cleanup;
        }
    }
    
    /* pad continuation area until block size */
    i = BLOCK_SIZE - (info->ce_len % BLOCK_SIZE);
    if (i > 0 && i < BLOCK_SIZE) {
        memset(padding, 0, i);
        ret = iso_write(t, padding, i);
    }
    
write_ce_field_cleanup:;    
    /* free ce_susp_fields */
    for (i = 0; i < info->n_ce_susp_fields; ++i) {
        free(info->ce_susp_fields[i]);
    }
    free(info->ce_susp_fields);
    info->ce_susp_fields = NULL;
    info->n_ce_susp_fields = 0;
    info->ce_len = 0;
    return ret;
}

