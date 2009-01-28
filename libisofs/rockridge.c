/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2007 Mario Danic
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#define Libisofs_new_nm_sl_cE yes

/* ts A90116 : libisofs.h eventually defines Libisofs_with_aaiP */
#include "libisofs.h"

#include "rockridge.h"
#include "node.h"
#include "ecma119_tree.h"
#include "writer.h"
#include "messages.h"
#include "image.h"

#ifdef Libisofs_with_aaiP
#include "aaip_0_2.h"
#endif

#include <string.h>


#ifdef Libisofs_with_aaiP

/* ts A90125 */
static
int susp_add_ES(Ecma119Image *t, struct susp_info *susp, int to_ce, int seqno);

#endif /* Libisofs_with_aaiP */


static
int susp_append(Ecma119Image *t, struct susp_info *susp, uint8_t *data)
{
    susp->n_susp_fields++;
    susp->susp_fields = realloc(susp->susp_fields, sizeof(void*)
                                * susp->n_susp_fields);
    if (susp->susp_fields == NULL) {
        return ISO_OUT_OF_MEM;
    }
    susp->susp_fields[susp->n_susp_fields - 1] = data;
    susp->suf_len += data[2];
    return ISO_SUCCESS;
}

static
int susp_append_ce(Ecma119Image *t, struct susp_info *susp, uint8_t *data)
{
    susp->n_ce_susp_fields++;
    susp->ce_susp_fields = realloc(susp->ce_susp_fields, sizeof(void*)
                                   * susp->n_ce_susp_fields);
    if (susp->ce_susp_fields == NULL) {
        return ISO_OUT_OF_MEM;
    }
    susp->ce_susp_fields[susp->n_ce_susp_fields - 1] = data;
    susp->ce_len += data[2];
    return ISO_SUCCESS;
}

static
uid_t px_get_uid(Ecma119Image *t, Ecma119Node *n)
{
    if (t->replace_uid) {
        return t->uid;
    } else {
        return n->node->uid;
    }
}

static
uid_t px_get_gid(Ecma119Image *t, Ecma119Node *n)
{
    if (t->replace_gid) {
        return t->gid;
    } else {
        return n->node->gid;
    }
}

static
mode_t px_get_mode(Ecma119Image *t, Ecma119Node *n)
{
    if ((n->type == ECMA119_DIR || n->type == ECMA119_PLACEHOLDER)) {
        if (t->replace_dir_mode) {
            return (n->node->mode & S_IFMT) | t->dir_mode;
        }
    } else {
        if (t->replace_file_mode) {
            return (n->node->mode & S_IFMT) | t->file_mode;
        }
    }
    return n->node->mode;
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
        return ISO_OUT_OF_MEM;
    }

    PX[0] = 'P';
    PX[1] = 'X';
    if (!t->rrip_version_1_10) {
        PX[2] = 44;
    } else {
        PX[2] = 36;
    }
    PX[3] = 1;
    iso_bb(&PX[4], px_get_mode(t, n), 4);
    iso_bb(&PX[12], n->nlink, 4);
    iso_bb(&PX[20], px_get_uid(t, n), 4);
    iso_bb(&PX[28], px_get_gid(t, n), 4);
    if (!t->rrip_version_1_10) {
        iso_bb(&PX[36], n->ino, 4);
    }

    return susp_append(t, susp, PX);
}

/**
 * Add to the given tree node a TF System Use Entry, used to record some
 * time stamps related to the file (RRIP, 4.1.6).
 */
static
int rrip_add_TF(Ecma119Image *t, Ecma119Node *n, struct susp_info *susp)
{
    IsoNode *iso;
    uint8_t *TF = malloc(5 + 3 * 7);
    if (TF == NULL) {
        return ISO_OUT_OF_MEM;
    }

    TF[0] = 'T';
    TF[1] = 'F';
    TF[2] = 5 + 3 * 7;
    TF[3] = 1;
    TF[4] = (1 << 1) | (1 << 2) | (1 << 3);
    
    iso = n->node;
    iso_datetime_7(&TF[5], t->replace_timestamps ? t->timestamp : iso->mtime,
                   t->always_gmt);
    iso_datetime_7(&TF[12], t->replace_timestamps ? t->timestamp : iso->atime,
                   t->always_gmt);
    iso_datetime_7(&TF[19], t->replace_timestamps ? t->timestamp : iso->ctime,
                   t->always_gmt);
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

    if (n->type != ECMA119_DIR || n->info.dir->real_parent == NULL) {
        /* should never occur */
        return ISO_ASSERT_FAILURE;
    }

    PL = malloc(12);
    if (PL == NULL) {
        return ISO_OUT_OF_MEM;
    }

    PL[0] = 'P';
    PL[1] = 'L';
    PL[2] = 12;
    PL[3] = 1;

    /* write the location of the real parent, already computed */
    iso_bb(&PL[4], n->info.dir->real_parent->info.dir->block, 4);
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
        return ISO_OUT_OF_MEM;
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
    int high_shift= 0;

    node = (IsoSpecial*)n->node;
    if (node->node.type != LIBISO_SPECIAL) {
        /* should never occur */
        return ISO_ASSERT_FAILURE;
    }

    PN = malloc(20);
    if (PN == NULL) {
        return ISO_OUT_OF_MEM;
    }

    PN[0] = 'P';
    PN[1] = 'N';
    PN[2] = 20;
    PN[3] = 1;

    /* (dev_t >> 32) causes compiler warnings on FreeBSD.
       RRIP 1.10 4.1.2 prescribes PN "Dev_t High" to be 0 on 32 bit dev_t.
    */
    if (sizeof(node->dev) > 4) {
        high_shift = 32;
        iso_bb(&PN[4], node->dev >> high_shift, 4);
    } else
        iso_bb(&PN[4], 0, 4);
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
        return ISO_ASSERT_FAILURE;
    }
    CL = malloc(12);
    if (CL == NULL) {
        return ISO_OUT_OF_MEM;
    }

    CL[0] = 'C';
    CL[1] = 'L';
    CL[2] = 12;
    CL[3] = 1;
    iso_bb(&CL[4], n->info.real_me->info.dir->block, 4);
    return susp_append(t, susp, CL);
}

/**
 * Convert a RR filename to the requested charset. On any conversion error, 
 * the original name will be used.
 */
static
char *get_rr_fname(Ecma119Image *t, const char *str)
{
    int ret;
    char *name;

    if (!strcmp(t->input_charset, t->output_charset)) {
        /* no conversion needed */
        return strdup(str);
    }

    ret = strconv(str, t->input_charset, t->output_charset, &name);
    if (ret < 0) {
        /* TODO we should check for possible cancelation */
        iso_msg_submit(t->image->id, ISO_FILENAME_WRONG_CHARSET, ret,
                  "Charset conversion error. Can't convert %s from %s to %s",
                  str, t->input_charset, t->output_charset);

        /* use the original name, it's the best we can do */
        name = strdup(str);
    }

    return name;
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
int rrip_add_NM(Ecma119Image *t, struct susp_info *susp, char *name, int size,
                int flags, int ce)
{
    uint8_t *NM = malloc(size + 5);
    if (NM == NULL) {
        return ISO_OUT_OF_MEM;
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
int rrip_SL_append_comp(size_t *n, uint8_t ***comps, char *s, int size, char fl)
{
    uint8_t *comp = malloc(size + 2);
    if (comp == NULL) {
        return ISO_OUT_OF_MEM;
    }

    (*n)++;
    comp[0] = fl;
    comp[1] = size;
    *comps = realloc(*comps, (*n) * sizeof(void*));
    if (*comps == NULL) {
        free(comp);
        return ISO_OUT_OF_MEM;
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
int rrip_add_SL(Ecma119Image *t, struct susp_info *susp, uint8_t **comp,
                size_t n, int ce)
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
                return ISO_OUT_OF_MEM;
            }

            SL[0] = 'S';
            SL[1] = 'L';
            SL[2] = total_comp_len + 5;
            SL[3] = 1;
            SL[4] = 1; /* CONTINUE */
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
                return ISO_ASSERT_FAILURE;
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
        return ISO_OUT_OF_MEM;
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


#ifdef Libisofs_with_aaiP

/* ts A90112 */
/*
   @param flag bit0= only account sizes in sua_free resp. ce_len
                     parameters susp and data may be NULL in this case
*/
static
int aaip_add_AA(Ecma119Image *t, struct susp_info *susp,
                uint8_t **data, size_t num_data,
                size_t *sua_free, size_t *ce_len, int flag)
{
    int ret, done = 0, len, es_extra = 0;
    uint8_t *aapt, *cpt;

    if (!t->aaip_susp_1_10)
        es_extra = 5;
    if (*sua_free < num_data + es_extra || *ce_len > 0) {
        *ce_len += num_data + es_extra;
    } else {
        *sua_free -= num_data + es_extra;
    }
    if (flag & 1)
        return ISO_SUCCESS;

    /* If AAIP enabled and announced by ER : Write ES field to announce AAIP */
    if (t->aaip && !t->aaip_susp_1_10) {
        ret = susp_add_ES(t, susp, (*ce_len > 0), 1);
        if (ret < 0)
            return ret;
    }

    aapt = *data;
    if (!(aapt[4] & 1)) {
        /* Single field can be handed over directly */
        if (*ce_len > 0) {
            ret = susp_append_ce(t, susp, aapt);
        } else {
            ret = susp_append(t, susp, aapt);
        }
        *data = NULL;
        return ISO_SUCCESS;
    }

    /* Multiple fields have to be handed over as single field copies */
    for (aapt = *data; !done; aapt += aapt[2]) {
        done = !(aapt[4] & 1);
        len = aapt[2];
        cpt = calloc(aapt[2], 1);
        if (cpt == NULL)
            return ISO_OUT_OF_MEM;
        memcpy(cpt, aapt, len);
        if (*ce_len > 0) {
            ret = susp_append_ce(t, susp, cpt);
        } else {
            ret = susp_append(t, susp, cpt);
        }
        if (ret == -1)
            return ret;
    }
    free(*data);
    *data = NULL;
    return ISO_SUCCESS;
}

#endif /* Libisofs_with_aaiP */


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
    unsigned char *ER;

    if (!t->rrip_version_1_10) {
        /*
        According to RRIP 1.12 this is the future form:
        4.3 "Specification of the ER System Use Entry Values for RRIP"
        talks of "IEEE_P1282" in each of the three strings and finally states
        "Note: Upon adoption as an IEEE standard, these lengths will each
         decrease by 1."
        So "IEEE_P1282" would be the new form, "RRIP_1991A" is the old form.
        */
        ER = malloc(182);
        if (ER == NULL) {
            return ISO_OUT_OF_MEM;
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
    } else {
        /*
        RRIP 1.09 and 1.10: 
        4.3 Specification of the ER System Use Field Values for RRIP
        The Extension Version number for the version of the RRIP defined herein
        shall be 1. The content of the Extension Identifier field shall be
        "RRIP_1991A". The Identifier Length shall be 10. The recommended
        content of the Extension Descriptor shall be "THE ROCK RIDGE
        INTERCHANGE PROTOCOL PROVIDES SUPPORT FOR POSIX FILE SYSTEM SEMANTICS."
        The corresponding Description Length is 84.
        The recommended content of the Extension Source shall be "PLEASE
        CONTACT DISC PUBLISHER FOR SPECIFICATION SOURCE.  SEE PUBLISHER
        IDENTIFIER IN PRIMARY VOLUME DESCRIPTOR FOR CONTACT INFORMATION."
        The corresponding Source Length is 135.
        */

        ER = malloc(237);
        if (ER == NULL) {
            return ISO_OUT_OF_MEM;
        }

        ER[0] = 'E';
        ER[1] = 'R';
        ER[2] = 237;
        ER[3] = 1;
        ER[4] = 10;
        ER[5] = 84;
        ER[6] = 135;
        ER[7] = 1;

        memcpy(&ER[8], "RRIP_1991A", 10);
        memcpy(&ER[18], "THE ROCK RIDGE INTERCHANGE PROTOCOL PROVIDES SUPPORT FOR POSIX FILE SYSTEM SEMANTICS", 84);
        memcpy(&ER[102], "PLEASE CONTACT DISC PUBLISHER FOR SPECIFICATION SOURCE.  SEE PUBLISHER IDENTIFIER IN PRIMARY VOLUME DESCRIPTOR FOR CONTACT INFORMATION.", 135);
    }

    /** This always goes to continuation area */
    return susp_append_ce(t, susp, ER);
}


static
int aaip_add_ER(Ecma119Image *t, struct susp_info *susp, char aa[2], int flag)
{
    unsigned char *AA;

    AA = malloc(160);
    if (AA == NULL) {
        return ISO_OUT_OF_MEM;
    }
    
    AA[0] = 'E';
    AA[1] = 'R';
    AA[2] = 160;
    AA[3] = 1;
    AA[4] = 9;
    AA[5] = 81;
    AA[6] = 62;
    AA[7] = 1;
    memcpy(AA + 8, "AAIP_0002", 9);
    AA[17] = aa[0];
    AA[18] = aa[1];
    memcpy(AA + 19,
           " PROVIDES VIA AAIP 0.2 SUPPORT FOR ARBITRARY FILE ATTRIBUTES"
           " IN ISO 9660 IMAGES", 79);
    memcpy(AA + 98,
           "PLEASE CONTACT THE LIBBURNIA PROJECT VIA LIBBURNIA-PROJECT.ORG",
           62);

    /** This always goes to continuation area */
    return susp_append_ce(t, susp, AA);
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
        return ISO_OUT_OF_MEM;
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
        return ISO_OUT_OF_MEM;
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


/* ts A90125 */
/**
 * SUSP 1.12: [...] shall specify as an 8-bit number the Extension
 * Sequence Number of the extension specification utilized in the entries
 * immediately following this System Use Entry. The Extension Sequence
 * Numbers of multiple extension specifications on a volume shall correspond to
 * the order in which their "ER" System Use Entries are recorded [...]
 */
static
int susp_add_ES(Ecma119Image *t, struct susp_info *susp, int to_ce, int seqno)
{
    unsigned char *ES = malloc(5);

    if (ES == NULL) {
        return ISO_OUT_OF_MEM;
    }
    ES[0] = 'E';
    ES[1] = 'S';
    ES[2] = (unsigned char) 5;
    ES[3] = (unsigned char) 1;
    ES[4] = (unsigned char) seqno;
    if (to_ce) {
        return susp_append_ce(t, susp, ES);
    } else {
        return susp_append(t, susp, ES);
    }
}


/* ts A90114 */

int aaip_xinfo_func(void *data, int flag)
{
    if (flag & 1) {
        free(data);
    }
    return 1;
}


/* ts A90117 */
/**
 * Compute SUA lentgth and eventual Continuation Area length of field NM and
 * eventually fields SL and AA. Because CA usage makes necessary the use of
 * a CE entry of 28 bytes in SUA, this computation fails if not the 28 bytes
 * are taken into account at start. In this case the caller should retry with
 * bit0 set. 
 * 
 * @param flag      bit0= assume CA usage (else return 0 on SUA overflow)
 * @return          1= ok, computation of *su_size and *ce is valid
 *                  0= not ok, CA usage is necessary but bit0 was not set
 *                     (*su_size and *ce stay unaltered in this case)
 *                 <0= error:
 *                 -1= not enough SUA space for 28 bytes of CE entry
 */
static
int susp_calc_nm_sl_aa(Ecma119Image *t, Ecma119Node *n, size_t space,
                       size_t *su_size, size_t *ce, int flag)
{
    char *name;
    size_t namelen, su_mem, ce_mem;

#ifdef Libisofs_with_aaiP
    /* ts A90112 */
    void *xipt;
    size_t num_aapt = 0, sua_free = 0;
    int ret;
#endif

    su_mem = *su_size;
    ce_mem = *ce;
    if (*ce > 0 && !(flag & 1))
        goto unannounced_ca;

    name = get_rr_fname(t, n->node->name);
    namelen = strlen(name);
    free(name);

    if (flag & 1) {
       /* Account for 28 bytes of CE field */
       if (*su_size + 28 > space)
           return -1;
       *su_size += 28;
    }

    /* NM entry */
    if (*su_size + 5 + namelen <= space) {
        /* ok, it fits in System Use Area */
        *su_size += 5 + namelen;
    } else {
        /* the NM will be divided in a CA */
        if (!(flag & 1))
            goto unannounced_ca;
        namelen = namelen - (space - *su_size - 5);
        *ce = 5 + namelen;
        *su_size = space;
    }
    if (n->type == ECMA119_SYMLINK) {
        /* 
         * for symlinks, we also need to write the SL
         */
        char *dest, *cur, *prev;
        size_t sl_len = 5;
        int cew = (*ce != 0); /* are we writing to CA ? */

        dest = get_rr_fname(t, ((IsoSymlink*)n->node)->dest);
        prev = dest;
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
                if (*su_size + sl_len + clen > space) {
                    /* 
                     * ok, we need a Continuation Area anyway
                     * TODO this can be handled better, but for now SL
                     * will be completelly moved into the CA
                     */
                    if (!(flag & 1))
                        goto unannounced_ca;
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

        free(dest);

        /* and finally write the pending SL field */
        if (!cew) {
            /* the whole SL fits into the SUA */
            *su_size += sl_len;
        } else {
            *ce += sl_len;
        }

    }

#ifdef Libisofs_with_aaiP
    /* ts A90112 */
    xipt = NULL;

#ifdef Libisofs_with_aaip_dummY

    num_aapt = 28;
    aaip_xinfo_func(NULL, 0); /* to avoid compiler warning */

#else /* Libisofs_with_aaip_dummY */

    /* obtain num_aapt from node */
    num_aapt = 0;

    if (t->aaip) {
        ret = iso_node_get_xinfo(n->node, aaip_xinfo_func, &xipt);
        if (ret == 1) {
           num_aapt = aaip_count_bytes((unsigned char *) xipt, 0);
        }
    }

#endif /* ! Libisofs_with_aaip_dummY */

	/* let the expert decide where to add num_aapt */
    if (num_aapt > 0) {
        sua_free = space - *su_size;
        aaip_add_AA(t, NULL, NULL, num_aapt, &sua_free, ce, 1);
        *su_size = space - sua_free;
        if (*ce > 0 && !(flag & 1))
            goto unannounced_ca;
    }

#endif /* Libisofs_with_aaiP */

    return 1;

unannounced_ca:;
    *su_size = su_mem;
    *ce = ce_mem;
    return 0;
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
size_t rrip_calc_len(Ecma119Image *t, Ecma119Node *n, int type, size_t space,
                     size_t *ce)
{
    size_t su_size;

#ifdef Libisofs_with_aaiP
    /* ts A90112 */
#ifndef Libisofs_new_nm_sl_cE
    void *xipt;
    size_t num_aapt = 0, sua_free = 0;
#endif /* Libisofs_new_nm_sl_cE */
    int ret;
#else /* Libisofs_with_aaiP */
#ifdef Libisofs_new_nm_sl_cE
    int ret;
#endif
#endif /* ! Libisofs_with_aaiP */

    /* space min is 255 - 33 - 37 = 185
     * At the same time, it is always an odd number, but we need to pad it
     * propertly to ensure the length of a directory record is a even number
     * (ECMA-119, 9.1.13). Thus, in fact the real space is always space - 1
     */
    space--;
    *ce = 0;

    su_size = 0;

#ifdef Libisofs_with_aaiP

    /* ts A90125 */
    /* If AAIP enabled and announced by ER : account for 5 bytes of ES */;
    if (t->aaip && !t->aaip_susp_1_10)
        su_size += 5;

#endif /* Libisofs_with_aaiP */

    /* PX and TF, we are sure they always fit in SUA */
    if (!t->rrip_version_1_10) {
        su_size += 44 + 26;
    } else {
        su_size += 36 + 26;
    }

    if (n->type == ECMA119_DIR) {
        if (n->info.dir->real_parent != NULL) {
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

#ifdef Libisofs_new_nm_sl_cE

        /* Try without CE */
        ret = susp_calc_nm_sl_aa(t, n, space, &su_size, ce, 0);
        if (ret == 0) /* Retry with CE */
            susp_calc_nm_sl_aa(t, n, space, &su_size, ce, 1);

#else /* Libisofs_new_nm_sl_cE */

        char *name = get_rr_fname(t, n->node->name);
        size_t namelen = strlen(name);
        free(name);

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
            char *dest, *cur, *prev;
            size_t sl_len = 5;
            int cew = (*ce != 0); /* are we writing to CE? */

            dest = get_rr_fname(t, ((IsoSymlink*)n->node)->dest);
            prev = dest;
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

            free(dest);

            /* and finally write the pending SL field */
            if (!cew) {
                /* the whole SL fits into the SUA */
                su_size += sl_len;
            } else {
                *ce += sl_len;
            }

        }

#ifdef Libisofs_with_aaiP
        /* ts A90112 */
        xipt = NULL;

#ifdef Libisofs_with_aaip_dummY

        num_aapt = 28;
        aaip_xinfo_func(NULL, 0); /* to avoid compiler warning */

#else /* Libisofs_with_aaip_dummY */

        /* obtain num_aapt from node */
        num_aapt = 0;
        if (t->aaip) {
            ret = iso_node_get_xinfo(n->node, aaip_xinfo_func, &xipt);
            if (ret == 1) {
               num_aapt = aaip_count_bytes((unsigned char *) xipt, 0);
            }
        }

#endif /* ! Libisofs_with_aaip_dummY */

	/* let the expert decide where to add num_aapt */
        if (num_aapt > 0) {
            sua_free = space - su_size;
            aaip_add_AA(t, NULL, NULL, num_aapt, &sua_free, ce, 1);
            su_size = space - sua_free;
        }

#endif /* Libisofs_with_aaiP */

#endif /* ! Libisofs_new_nm_sl_cE */

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
            *ce = 182; /* ER of RRIP */

#ifdef Libisofs_with_aaiP
            /* ts A90113 */

#ifdef Libisofs_with_aaip_dummY

            if (1) {
#else /* Libisofs_with_aaip_dummY */

            if (t->aaip) {

#endif /* ! Libisofs_with_aaip_dummY */

                *ce += 160; /* ER of AAIP */
            }

#endif /* Libisofs_with_aaiP */

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
    char *name = NULL;
    char *dest = NULL;

#ifdef Libisofs_with_aaiP
    /* ts A90112 */
    uint8_t *aapt;
    void *xipt;
    size_t num_aapt= 0;
#endif
    size_t aaip_er_len= 0;

#ifdef Libisofs_new_nm_sl_cE
    size_t su_size_pd, ce_len_pd; /* predicted sizes of SUA and CA */
    int ce_is_predicted = 0;
#endif

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

#ifdef Libisofs_with_aaiP

    /* ts A90125 */
    /* If AAIP enabled and announced by ER : Announce RRIP by ES */
    if (t->aaip && !t->aaip_susp_1_10) {
        ret = susp_add_ES(t, info, 0, 0);
        if (ret < 0)
            goto add_susp_cleanup;
    }

#endif /* Libisofs_with_aaiP */

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
        if (n->info.dir->real_parent != NULL) {
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
        size_t sua_free; /* free space in the SUA */
        int nm_type = 0; /* 0 whole entry in SUA, 1 part in CE */
        size_t ce_len = 0; /* len of the CE */
        size_t namelen;

        /* this two are only defined for symlinks */
        uint8_t **comps= NULL; /* components of the SL field */
        size_t n_comp = 0; /* number of components */

        name = get_rr_fname(t, n->node->name);
        namelen = strlen(name);

        sua_free = space - info->suf_len;

#ifdef Libisofs_new_nm_sl_cE
        /* ts A90117 */
        /* Try whether NM, SL, AA will fit into SUA */
        su_size_pd = info->suf_len;
        ce_len_pd = ce_len;
        ret = susp_calc_nm_sl_aa(t, n, space, &su_size_pd, &ce_len_pd, 0);
        if (ret == 0) { /* Have to use CA. 28 bytes of CE are necessary */
            susp_calc_nm_sl_aa(t, n, space, &su_size_pd, &ce_len_pd, 1);
            sua_free -= 28;
            ce_is_predicted = 1;
        }

#endif /* ! Libisofs_new_nm_sl_cE */

        /* NM entry */
        if (5 + namelen <= sua_free) {
            /* ok, it fits in System Use Area */
            sua_free -= (5 + namelen);
            nm_type = 0;
        } else {
            /* the NM will be divided in a CE */
            nm_type = 1;

#ifdef Libisofs_new_nm_sl_cE
            namelen = namelen - (sua_free - 5);
#else
            namelen = namelen - (sua_free - 5 - 28);
#endif

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

            dest = get_rr_fname(t, ((IsoSymlink*)n->node)->dest);
            prev = dest;
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
                }
                if (clen == 1 && prev[0] == '.') {
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

#ifdef Libisofs_new_nm_sl_cE

                        /* ts A90117 */
                        /* sua_free, ce_len, nm_type already account for CE */
                        cew = 1;

#else /* Libisofs_new_nm_sl_cE */

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

#endif /* ! Libisofs_new_nm_sl_cE */

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
                                ret = rrip_SL_append_comp(&n_comp, &comps, prev
                                        + fit, clen - fit - 2, 0);
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
                                ret = rrip_SL_append_comp(&n_comp, &comps, prev
                                        + 248, strlen(prev + 248), 0x00);
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

#ifdef Libisofs_new_nm_sl_cE
            namelen = space - info->suf_len - 28 * (!!ce_is_predicted) - 5;
#else
            namelen = space - info->suf_len - 28 - 5;
#endif

            ret = rrip_add_NM(t, info, name, namelen, 1, 0);
            if (ret < 0) {
                goto add_susp_cleanup;
            }
        }

#ifdef Libisofs_new_nm_sl_cE

        if (ce_is_predicted) {
            /* Add the CE entry */
            ret = susp_add_CE(t, ce_len_pd, info);

#else /* Libisofs_new_nm_sl_cE */

        if (ce_len > 0) {
            /* Add the CE entry */
            ret = susp_add_CE(t, ce_len, info);

#endif /* ! Libisofs_new_nm_sl_cE */

            if (ret < 0) {
                goto add_susp_cleanup;
            }
        }

        if (nm_type == 1) {
            /* 
             * ..and the part that goes to continuation area.
             */
            ret = rrip_add_NM(t, info, name + namelen, strlen(name + namelen),
                              0, 1);
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

#ifdef Libisofs_with_aaiP

#ifdef Libisofs_with_aaip_dummY
/* ts A90112 */

{
        static uint8_t dummy_aa[28]= {
            'A', 'A',  28,   1,   0,
             0,   0,
             0,  19,  0x16,
                      0x2E,   4, 'l', 'i', 's', 'a',
                      0x34, 
                      0x4E,   7, 't', 'o', 'o', 'l', 'i', 'e', 's',
                      0x54, 
                      0x64
        };

        num_aapt = 28;
        aapt = malloc(num_aapt);
        memcpy(aapt, dummy_aa, num_aapt);

        ret = aaip_add_AA(t, info, &aapt, num_aapt, &sua_free, &ce_len, 0);
        if (ret < 0) {
            goto add_susp_cleanup;
        }
        /* aapt is NULL now and the memory is owned by t */
}

#else /* Libisofs_with_aaip_dummY */
/* ts A90114 */

        /* Obtain AA field string from node
           and write it to directory entry or CE area.
        */
        ret = ISO_SUCCESS;
        num_aapt = 0;

        if (t->aaip) {
            ret = iso_node_get_xinfo(n->node, aaip_xinfo_func, &xipt);
            if (ret == 1) {
                num_aapt = aaip_count_bytes((unsigned char *) xipt, 0);
                if (num_aapt > 0) {
                    aapt = malloc(num_aapt);
                    if (aapt == NULL) {
                        ret = ISO_OUT_OF_MEM;
                        goto add_susp_cleanup;
                    }
                    memcpy(aapt, xipt, num_aapt);
                    ret = aaip_add_AA(t, info, &aapt, num_aapt,
                                      &sua_free, &ce_len, 0);
                    if (ret < 0) {
                        goto add_susp_cleanup;
                    }
                    /* aapt is NULL now and the memory is owned by t */
                }
            }
        }

#endif /* ! Libisofs_with_aaip_dummY */

#endif /* Libisofs_with_aaiP */

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

#ifdef Libisofs_with_aaiP
            /* ts A90113 */

#ifdef Libisofs_with_aaip_dummY

            if (1) {
            
#else /* Libisofs_with_aaip_dummY */

            if (t->aaip && !t->aaip_susp_1_10) {

#endif /* ! Libisofs_with_aaip_dummY */

                aaip_er_len = 160;
            }

#endif /* Libisofs_with_aaiP */

            ret = susp_add_CE(t, 182 + aaip_er_len, info);
                                                    /* 182 is RRIP-ER length */
            if (ret < 0) {
                goto add_susp_cleanup;
            }
            ret = rrip_add_ER(t, info);
            if (ret < 0) {
                goto add_susp_cleanup;
            }

#ifdef Libisofs_with_aaiP
            /* ts A90113 */

#ifdef Libisofs_with_aaip_dummY

            if (1) {
            
#else /* Libisofs_with_aaip_dummY */

            if (t->aaip && !t->aaip_susp_1_10) {
    
#endif /* ! Libisofs_with_aaip_dummY */

                ret = aaip_add_ER(t, info, "AA", 0);
                if (ret < 0) {
                    goto add_susp_cleanup;
                }
            }

#endif /* Libisofs_with_aaiP */

        }
    }


    /*
     * The System Use field inside the directory record must be padded if
     * it is an odd number (ECMA-119, 9.1.13)
     */
    info->suf_len += (info->suf_len % 2);

    free(name);
    free(dest);
    return ISO_SUCCESS;

    add_susp_cleanup: ;
    free(name);
    free(dest);
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
    int ret= ISO_SUCCESS;

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

    write_ce_field_cleanup: ;
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

