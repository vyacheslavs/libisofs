/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2007 Mario Danic
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "joliet.h"
#include "messages.h"
#include "writer.h"
#include "error.h"
#include "image.h"
#include "filesrc.h"

#include <stdlib.h>
#include <string.h>

static
int get_joliet_name(Ecma119Image *t, IsoNode *iso, uint16_t **name)
{
    int ret;
    uint16_t *ucs_name;
    uint16_t *jname = NULL;

    if (iso->name == NULL) {
        /* it is not necessarily an error, it can be the root */
        return ISO_SUCCESS;
    }

    ret = str2ucs(t->input_charset, iso->name, &ucs_name);
    if (ret < 0) {
        iso_msg_debug(t->image->messenger, "Can't convert %s", iso->name);
        return ret;
    }

    // TODO add support for relaxed constraints
    if (iso->type == LIBISO_DIR) {
        jname = iso_j_dir_id(ucs_name);
    } else {
        jname = iso_j_file_id(ucs_name);
    }
    free(ucs_name);
    if (jname != NULL) {
        *name = jname;
        return ISO_SUCCESS;
    } else {
        /* 
         * only possible if mem error, as check for empty names is done
         * in public tree
         */
        return ISO_MEM_ERROR;
    }
}

static
void joliet_node_free(JolietNode *node)
{
    if (node == NULL) {
        return;
    }
    if (node->type == JOLIET_DIR) {
        int i;
        for (i = 0; i < node->info.dir.nchildren; i++) {
            joliet_node_free(node->info.dir.children[i]);
        }
        free(node->info.dir.children);
        //free(node->info.dir);
    }
    iso_node_unref(node->node);
    free(node);
}

/**
 * Create a low level Joliet node
 * @return
 *      1 success, 0 ignored, < 0 error
 */
static
int create_node(Ecma119Image *t, IsoNode *iso, JolietNode **node)
{
    int ret;
    JolietNode *joliet;

    joliet = calloc(1, sizeof(JolietNode));
    if (joliet == NULL) {
        return ISO_MEM_ERROR;
    }

    if (iso->type == LIBISO_DIR) {
        IsoDir *dir = (IsoDir*) iso;
        joliet->info.dir.children = calloc(sizeof(void*), dir->nchildren);
        if (joliet->info.dir.children == NULL) {
            free(joliet);
            return ISO_MEM_ERROR;
        }
        joliet->type = JOLIET_DIR;
    } else if (iso->type == LIBISO_FILE) {
        /* it's a file */
        off_t size;
        IsoFileSrc *src;
        IsoFile *file = (IsoFile*) iso;

        size = iso_stream_get_size(file->stream);
        if (size > (off_t)0xffffffff) {
            iso_msg_note(t->image->messenger, LIBISO_FILE_IGNORED,
                         "File \"%s\" can't be added to image because is "
                         "greater than 4GB", iso->name);
            free(joliet);
            return 0;
        }

        ret = iso_file_src_create(t, file, &src);
        if (ret < 0) {
            free(joliet);
            return ret;
        }
        joliet->info.file = src;
        joliet->type = JOLIET_FILE;
    } else {
        /* should never happen */
        //TODO handle boot nodes?!?
        free(joliet);
        return ISO_ERROR;
    }

    /* take a ref to the IsoNode */
    joliet->node = iso;
    iso_node_ref(iso);

    *node = joliet;
    return ISO_SUCCESS;
}

/**
 * Create the low level Joliet tree from the high level ISO tree.
 * 
 * @return
 *      1 success, 0 file ignored, < 0 error
 */
static
int create_tree(Ecma119Image *t, IsoNode *iso, JolietNode **tree, int pathlen)
{
    int ret, max_path;
    JolietNode *node = NULL;
    uint16_t *jname = NULL;

    if (t == NULL || iso == NULL || tree == NULL) {
        return ISO_NULL_POINTER;
    }

    if (iso->hidden & LIBISO_HIDE_ON_JOLIET) {
        /* file will be ignored */
        return 0;
    }
    ret = get_joliet_name(t, iso, &jname);
    if (ret < 0) {
        return ret;
    }
    max_path = pathlen + 1 + (jname ? ucslen(jname) * 2 : 0);
    if (!t->joliet_longer_paths && max_path > 240) {
        /*
         * Wow!! Joliet is even more restrictive than plain ISO-9660,
         * that allows up to 255 bytes!!
         */
        iso_msg_note(t->image->messenger, LIBISO_FILE_IGNORED,
                     "File \"%s\" can't be added to Joliet tree, because "
                     "its path length is larger than 240", iso->name);
        free(jname);
        return 0;
    }

    switch (iso->type) {
    case LIBISO_FILE:
        ret = create_node(t, iso, &node);
        break;
    case LIBISO_DIR:
        {
            IsoNode *pos;
            IsoDir *dir = (IsoDir*)iso;
            ret = create_node(t, iso, &node);
            if (ret < 0) {
                free(jname);
                return ret;
            }
            pos = dir->children;
            while (pos) {
                int cret;
                JolietNode *child;
                cret = create_tree(t, pos, &child, max_path);
                if (cret < 0) {
                    /* error */
                    joliet_node_free(node);
                    ret = cret;
                    break;
                } else if (cret == ISO_SUCCESS) {
                    /* add child to this node */
                    int nchildren = node->info.dir.nchildren++;
                    node->info.dir.children[nchildren] = child;
                    child->parent = node;
                }
                pos = pos->next;
            }
        }
        break;
    case LIBISO_BOOT:
        //TODO
        return 0;
        break;
    case LIBISO_SYMLINK:
    case LIBISO_SPECIAL:
        iso_msg_note(t->image->messenger, LIBISO_JOLIET_WRONG_FILE_TYPE, 
                     "Can't add %s to Joliet tree. This kind of files can only"
                     " be added to a Rock Ridget tree. Skipping.", iso->name);
        ret = 0;
        break;
    default:
        /* should never happen */
        return ISO_ERROR;
    }
    if (ret <= 0) {
        free(jname);
        return ret;
    }
    node->name = jname;
    *tree = node;
    return ISO_SUCCESS;
}

static int
cmp_node(const void *f1, const void *f2)
{
    JolietNode *f = *((JolietNode**)f1);
    JolietNode *g = *((JolietNode**)f2);
    return ucscmp(f->name, g->name);
}

static 
void sort_tree(JolietNode *root)
{
    size_t i;

    qsort(root->info.dir.children, root->info.dir.nchildren, 
          sizeof(void*), cmp_node);
    for (i = 0; i < root->info.dir.nchildren; i++) {
        JolietNode *child = root->info.dir.children[i];
        if (child->type == JOLIET_DIR)
            sort_tree(child);
    }
}

static
int joliet_tree_create(Ecma119Image *t)
{
    int ret;
    JolietNode *root;
    
    if (t == NULL) {
        return ISO_NULL_POINTER;
    }

    ret = create_tree(t, (IsoNode*)t->image->root, &root, 0);
    if (ret <= 0) {
        if (ret == 0) {
            /* unexpected error, root ignored!! This can't happen */
            ret = ISO_ERROR;
        }
        return ret;
    }
    
    /* the Joliet tree is stored in Ecma119Image target */
    t->joliet_root = root;

    iso_msg_debug(t->image->messenger, "Sorting the Joliet tree...");
    sort_tree(root);

    //iso_msg_debug(t->image->messenger, "Mangling Joliet names...");
    // FIXME ret = mangle_tree(t, 1);

    return ISO_SUCCESS;
}

/**
 * Compute the size of a directory entry for a single node
 */
static
size_t calc_dirent_len(Ecma119Image *t, JolietNode *n)
{
    /* note than name len is always even, so we always need the pad byte */
    int ret = n->name ? ucslen(n->name) * 2 + 34 : 34;
    if (n->type == JOLIET_FILE && !t->omit_version_numbers) {
        /* take into account version numbers */
        ret += 4;
    }
    return ret;
}

/**
 * Computes the total size of all directory entries of a single joliet dir.
 * This is like ECMA-119 6.8.1.1, but taking care that names are stored in
 * UCS.
 */
static
size_t calc_dir_size(Ecma119Image *t, JolietNode *dir)
{
    size_t i, len;

    /* size of "." and ".." entries */
    len = 34 + 34;

    for (i = 0; i < dir->info.dir.nchildren; ++i) {
        size_t remaining;
        JolietNode *child = dir->info.dir.children[i];
        size_t dirent_len = calc_dirent_len(t, child);
        remaining = BLOCK_SIZE - (len % BLOCK_SIZE);
        if (dirent_len > remaining) {
            /* child directory entry doesn't fit on block */
            len += remaining + dirent_len;
        } else {
            len += dirent_len;
        }
    }
    /* cache the len */
    dir->info.dir.len = len;
    return len;
}

static
void calc_dir_pos(Ecma119Image *t, JolietNode *dir)
{
    size_t i, len;

    t->joliet_ndirs++;
    dir->info.dir.block = t->curblock;
    len = calc_dir_size(t, dir);
    t->curblock += div_up(len, BLOCK_SIZE);
    for (i = 0; i < dir->info.dir.nchildren; i++) {
        JolietNode *child = dir->info.dir.children[i];
        if (child->type == JOLIET_DIR) {
            calc_dir_pos(t, child);
        }
    }
}

/**
 * Compute the length of the joliet path table, in bytes.
 */
static
uint32_t calc_path_table_size(JolietNode *dir)
{
    uint32_t size;
    size_t i;

    /* size of path table for this entry */
    size = 8;
    size += dir->name ? ucslen(dir->name) * 2 : 2;

    /* and recurse */
    for (i = 0; i < dir->info.dir.nchildren; i++) {
        JolietNode *child = dir->info.dir.children[i];
        if (child->type == JOLIET_DIR) {
            size += calc_path_table_size(child);
        }
    }
    return size;
}

static
int joliet_writer_compute_data_blocks(IsoImageWriter *writer)
{
    Ecma119Image *t;
    uint32_t path_table_size;

    if (writer == NULL) {
        return ISO_MEM_ERROR;
    }

    t = writer->target;

    /* compute position of directories */
    iso_msg_debug(t->image->messenger, 
                  "Computing position of Joliet dir structure");
    t->joliet_ndirs = 0;
    calc_dir_pos(t, t->joliet_root);

    /* compute length of pathlist */
    iso_msg_debug(t->image->messenger, "Computing length of Joliet pathlist");
    path_table_size = calc_path_table_size(t->joliet_root);

    /* compute location for path tables */
    t->joliet_l_path_table_pos = t->curblock;
    t->curblock += div_up(path_table_size, BLOCK_SIZE);
    t->joliet_m_path_table_pos = t->curblock;
    t->curblock += div_up(path_table_size, BLOCK_SIZE);
    t->joliet_path_table_size = path_table_size;

    return ISO_SUCCESS;
}

/**
 * Write a single directory record for Joliet. It is like (ECMA-119, 9.1),
 * but file identifier is stored in UCS.
 * 
 * @param file_id
 *     if >= 0, we use it instead of the filename (for "." and ".." entries).
 * @param len_fi
 *     Computed length of the file identifier. Total size of the directory
 *     entry will be len + 34 (ECMA-119, 9.1.12), as padding is always needed
 */
static
void write_one_dir_record(Ecma119Image *t, JolietNode *node, int file_id,
                          uint8_t *buf, size_t len_fi)
{
    uint32_t len;
    uint32_t block;
    uint8_t len_dr; /*< size of dir entry */
    uint8_t *name = (file_id >= 0) ? (uint8_t*)&file_id
            : (uint8_t*)node->name;

    struct ecma119_dir_record *rec = (struct ecma119_dir_record*)buf;

    len_dr = 33 + len_fi + (len_fi % 2 ? 0 : 1);

    memcpy(rec->file_id, name, len_fi);

    if (node->type == JOLIET_FILE && !t->omit_version_numbers) {
        len_dr += 4;
        rec->file_id[len_fi++] = 0;
        rec->file_id[len_fi++] = ';';
        rec->file_id[len_fi++] = 0;
        rec->file_id[len_fi++] = '1';
    }

    if (node->type == JOLIET_DIR) {
        /* use the cached length */
        len = node->info.dir.len;
        block = node->info.dir.block;
    } else if (node->type == JOLIET_FILE) {
        len = iso_file_src_get_size(node->info.file);
        block = node->info.file->block;
    } else {
        //TODO el-torito???!?
        /* 
         * for nodes other than files and dirs, we set both 
         * len and block to 0 
         */
        len = 0;
        block = 0;
    }

    /*
     * For ".." entry we need to write the parent info!
     */
    if (file_id == 1 && node->parent)
        node = node->parent;

    rec->len_dr[0] = len_dr;
    iso_bb(rec->block, block, 4);
    iso_bb(rec->length, len, 4);
    iso_datetime_7(rec->recording_time, t->now);
    rec->flags[0] = (node->type == JOLIET_DIR) ? 2 : 0;
    iso_bb(rec->vol_seq_number, 1, 2);
    rec->len_fi[0] = len_fi;
}

static
int joliet_writer_write_vol_desc(IsoImageWriter *writer)
{
    IsoImage *image;
    Ecma119Image *t;
    struct ecma119_sup_vol_desc vol;

    uint16_t *vol_id = NULL, *pub_id = NULL, *data_id = NULL;
    uint16_t *volset_id = NULL, *system_id = NULL, *application_id = NULL;
    uint16_t *copyright_file_id = NULL, *abstract_file_id = NULL;
    uint16_t *biblio_file_id = NULL;

    if (writer == NULL) {
        return ISO_MEM_ERROR;
    }

    t = writer->target;
    image = t->image;

    iso_msg_debug(image->messenger, "Write SVD for Joliet");

    memset(&vol, 0, sizeof(struct ecma119_sup_vol_desc));

    str2ucs(t->input_charset, image->volume_id, &vol_id);
    str2ucs(t->input_charset, image->publisher_id, &pub_id);
    str2ucs(t->input_charset, image->data_preparer_id, &data_id);
    str2ucs(t->input_charset, image->volset_id, &volset_id);

    str2ucs(t->input_charset, image->system_id, &system_id);
    str2ucs(t->input_charset, image->application_id, &application_id);
    str2ucs(t->input_charset, image->copyright_file_id, &copyright_file_id);
    str2ucs(t->input_charset, image->abstract_file_id, &abstract_file_id);
    str2ucs(t->input_charset, image->biblio_file_id, &biblio_file_id);

    vol.vol_desc_type[0] = 2;
    memcpy(vol.std_identifier, "CD001", 5);
    vol.vol_desc_version[0] = 1;
    if (vol_id) {
        ucsncpy((uint16_t*)vol.volume_id, vol_id, 32);
    }
    
    /* make use of UCS-2 Level 3 */
    memcpy(vol.esc_sequences, "%/E", 3);

    iso_bb(vol.vol_space_size, t->vol_space_size, 4);
    iso_bb(vol.vol_set_size, 1, 2);
    iso_bb(vol.vol_seq_number, 1, 2);
    iso_bb(vol.block_size, BLOCK_SIZE, 2);
    iso_bb(vol.path_table_size, t->joliet_path_table_size, 4);
    iso_lsb(vol.l_path_table_pos, t->joliet_l_path_table_pos, 4);
    iso_msb(vol.m_path_table_pos, t->joliet_m_path_table_pos, 4);

    write_one_dir_record(t, t->joliet_root, 0, vol.root_dir_record, 1);

    if (volset_id)
        ucsncpy((uint16_t*)vol.vol_set_id, volset_id, 128);
    if (pub_id)
        ucsncpy((uint16_t*)vol.publisher_id, pub_id, 128);
    if (data_id)
        ucsncpy((uint16_t*)vol.data_prep_id, data_id, 128);
    
    if (system_id)
        ucsncpy((uint16_t*)vol.system_id, system_id, 32);

    if (application_id)
        ucsncpy((uint16_t*)vol.application_id, application_id, 128);
    if (copyright_file_id)
        ucsncpy((uint16_t*)vol.copyright_file_id, copyright_file_id, 37);
    if (abstract_file_id)
        ucsncpy((uint16_t*)vol.abstract_file_id, abstract_file_id, 37);
    if (biblio_file_id)
        ucsncpy((uint16_t*)vol.bibliographic_file_id, biblio_file_id, 37);

    iso_datetime_17(vol.vol_creation_time, t->now);
    iso_datetime_17(vol.vol_modification_time, t->now);
    iso_datetime_17(vol.vol_effective_time, t->now);
    vol.file_structure_version[0] = 1;

    free(vol_id);
    free(volset_id);
    free(pub_id);
    free(data_id);
    free(system_id);
    free(application_id);
    free(copyright_file_id);
    free(abstract_file_id);
    free(biblio_file_id);

    /* Finally write the Volume Descriptor */
    return iso_write(t, &vol, sizeof(struct ecma119_sup_vol_desc));
}

static
int write_one_dir(Ecma119Image *t, JolietNode *dir)
{
    int ret;
    uint8_t buffer[BLOCK_SIZE];
    size_t i;
    size_t fi_len, len;

    /* buf will point to current write position on buffer */
    uint8_t *buf = buffer;

    /* initialize buffer with 0s */
    memset(buffer, 0, BLOCK_SIZE);

    /* write the "." and ".." entries first */
    write_one_dir_record(t, dir, 0, buf, 1);
    buf += 34;
    write_one_dir_record(t, dir, 1, buf, 1);
    buf += 34;

    for (i = 0; i < dir->info.dir.nchildren; i++) {
        JolietNode *child = dir->info.dir.children[i];

        /* compute len of directory entry */
        fi_len = ucslen(child->name) * 2;
        len = fi_len + 34;
        if (child->type == JOLIET_FILE && !t->omit_version_numbers) {
            len += 4;
        }

        if ( (buf + len - buffer) > BLOCK_SIZE) {
            /* dir doesn't fit in current block */
            ret = iso_write(t, buffer, BLOCK_SIZE);
            if (ret < 0) {
                return ret;
            }
            memset(buffer, 0, BLOCK_SIZE);
            buf = buffer;
        }
        /* write the directory entry in any case */
        write_one_dir_record(t, child, -1, buf, fi_len);
        buf += len;
    }

    /* write the last block */
    ret = iso_write(t, buffer, BLOCK_SIZE);
    return ret;
}

static
int write_dirs(Ecma119Image *t, JolietNode *root)
{
    int ret;
    size_t i;

    /* write all directory entries for this dir */
    ret = write_one_dir(t, root);
    if (ret < 0) {
        return ret;
    }

    /* recurse */
    for (i = 0; i < root->info.dir.nchildren; i++) {
        JolietNode *child = root->info.dir.children[i];
        if (child->type == JOLIET_DIR) {
            ret = write_dirs(t, child);
            if (ret < 0) {
                return ret;
            }
        }
    }
    return ISO_SUCCESS;
}

static
int write_path_table(Ecma119Image *t, JolietNode **pathlist, int l_type)
{
    size_t i, len;
    uint8_t buf[256]; /* 256 is just a convenient size larger enought */
    struct ecma119_path_table_record *rec;
    void (*write_int)(uint8_t*, uint32_t, int);
    JolietNode *dir;
    uint32_t path_table_size;
    int parent = 0;
    int ret= ISO_SUCCESS;

    path_table_size = 0;
    write_int = l_type ? iso_lsb : iso_msb;

    for (i = 0; i < t->joliet_ndirs; i++) {
        dir = pathlist[i];

        /* find the index of the parent in the table */
        while ((i) && pathlist[parent] != dir->parent) {
            parent++;
        }

        /* write the Path Table Record (ECMA-119, 9.4) */
        memset(buf, 0, 256);
        rec = (struct ecma119_path_table_record*) buf;
        rec->len_di[0] = dir->parent ? (uint8_t) ucslen(dir->name) * 2 : 1;
        rec->len_xa[0] = 0;
        write_int(rec->block, dir->info.dir.block, 4);
        write_int(rec->parent, parent + 1, 2);
        if (dir->parent) {
            memcpy(rec->dir_id, dir->name, rec->len_di[0]);
        }
        len = 8 + rec->len_di[0] + (rec->len_di[0] % 2);
        ret = iso_write(t, buf, len);
        if (ret < 0) {
            /* error */
            return ret;
        }
        path_table_size += len;
    }

    /* we need to fill the last block with zeros */
    path_table_size %= BLOCK_SIZE;
    if (path_table_size) {
        uint8_t zeros[BLOCK_SIZE];
        len = BLOCK_SIZE - path_table_size;
        memset(zeros, 0, len);
        ret = iso_write(t, zeros, len);
    }
    return ret;
}

static
int write_path_tables(Ecma119Image *t)
{
    int ret;
    size_t i, j, cur;
    JolietNode **pathlist;

    iso_msg_debug(t->image->messenger, "Writing Joliet Path tables");

    /* allocate temporal pathlist */
    pathlist = malloc(sizeof(void*) * t->joliet_ndirs);
    if (pathlist == NULL) {
        return ISO_MEM_ERROR;
    }
    pathlist[0] = t->joliet_root;
    cur = 1;

    for (i = 0; i < t->joliet_ndirs; i++) {
        JolietNode *dir = pathlist[i];
        for (j = 0; j < dir->info.dir.nchildren; j++) {
            JolietNode *child = dir->info.dir.children[j];
            if (child->type == JOLIET_DIR) {
                pathlist[cur++] = child;
            }
        }
    }

    /* Write L Path Table */
    ret = write_path_table(t, pathlist, 1);
    if (ret < 0) {
        goto write_path_tables_exit;
    }

    /* Write L Path Table */
    ret = write_path_table(t, pathlist, 0);

    write_path_tables_exit: ;
    free(pathlist);
    return ret;
}

static
int joliet_writer_write_data(IsoImageWriter *writer)
{
    int ret;
    Ecma119Image *t;

    if (writer == NULL) {
        return ISO_NULL_POINTER;
    }
    t = writer->target;

    /* first of all, we write the directory structure */
    ret = write_dirs(t, t->joliet_root);
    if (ret < 0) {
        return ret;
    }

    /* and write the path tables */
    ret = write_path_tables(t);

    return ret;
}

static
int joliet_writer_free_data(IsoImageWriter *writer)
{
    /* free the Joliet tree */
    Ecma119Image *t = writer->target;
    joliet_node_free(t->joliet_root);
    return ISO_SUCCESS;
}

int joliet_writer_create(Ecma119Image *target)
{
    int ret;
    IsoImageWriter *writer;

    writer = malloc(sizeof(IsoImageWriter));
    if (writer == NULL) {
        return ISO_MEM_ERROR;
    }

    writer->compute_data_blocks = joliet_writer_compute_data_blocks;
    writer->write_vol_desc = joliet_writer_write_vol_desc;
    writer->write_data = joliet_writer_write_data;
    writer->free_data = joliet_writer_free_data;
    writer->data = NULL; //TODO store joliet tree here
    writer->target = target;

    iso_msg_debug(target->image->messenger, 
                  "Creating low level Joliet tree...");
    ret = joliet_tree_create(target);
    if (ret < 0) {
        return ret;
    }

    /* add this writer to image */
    target->writers[target->nwriters++] = writer;

    /* we need the volume descriptor */
    target->curblock++;
    return ISO_SUCCESS;
}
