/*
 * Unit test for util.h
 * 
 * This test utiliy functions
 */
#include "test.h"
#include "ecma119_tree.h"
#include "rockridge.h"
#include "node.h"

#include <string.h>

static void test_rrip_calc_len_file()
{
    IsoFile *file;
    Ecma119Node *node;
    Ecma119Image t;
    size_t sua_len = 0, ce_len = 0;
    
    memset(&t, 0, sizeof(Ecma119Image));
    t.input_charset = "UTF-8";
    t.output_charset = "UTF-8";
    
    file = malloc(sizeof(IsoFile));
    CU_ASSERT_PTR_NOT_NULL_FATAL(file);
    file->msblock = 0;
    file->sort_weight = 0;
    file->stream = NULL; /* it is not needed here */
    file->node.type = LIBISO_FILE;
    
    node = malloc(sizeof(Ecma119Node));
    CU_ASSERT_PTR_NOT_NULL_FATAL(node);
    node->node = (IsoNode*)file;
    node->parent = (Ecma119Node*)0x55555555; /* just to make it not NULL */
    node->info.file = NULL; /* it is not needed here */ 
    node->type = ECMA119_FILE;
    
    /* Case 1. Name fit in System Use field */
    file->node.name = "a small name.txt";
    node->iso_name = "A_SMALL_.TXT";
    
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 0);
    CU_ASSERT_EQUAL(sua_len, 44 + (5 + 16) + (5 + 3*7) + 1);
    
    /* Case 2. Name fits exactly */
    file->node.name = "a big name, with 133 characters, that it is the max "
                      "that fits in System Use field of the directory record "
                      "PADPADPADADPADPADPADPAD.txt";
    node->iso_name = "A_BIG_NA.TXT";
    
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 0);
    /* note that 254 is the max length of a directory record, as it needs to
     * be an even number */
    CU_ASSERT_EQUAL(sua_len, 254 - 46);
    
    /* case 3. A name just 1 character too big to fit in SUA */
    file->node.name = "a big name, with 133 characters, that it is the max "
                      "that fits in System Use field of the directory record "
                      "PADPADPADADPADPADPADPAD1.txt";
    node->iso_name = "A_BIG_NA.TXT";
    
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    /* 28 (the chars moved to include the CE entry) + 5 (header of NM in CE) +
     * 1 (the char that originally didn't fit) */
    CU_ASSERT_EQUAL(ce_len, 28 + 5 + 1);
    /* note that 254 is the max length of a directory record, as it needs to
     * be an even number */
    CU_ASSERT_EQUAL(sua_len, 254 - 46);
    
    /* case 4. A 255 characters name */
    file->node.name = "a big name, with 255 characters, that it is the max "
        "that a POSIX filename can have. PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP"
        "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP"
        "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP";
    node->iso_name = "A_BIG_NA.TXT";
    
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    /* 150 + 5 (header + characters that don't fit in sua) */
    CU_ASSERT_EQUAL(ce_len, 150 + 5);
    /* note that 254 is the max length of a directory record, as it needs to
     * be an even number */
    CU_ASSERT_EQUAL(sua_len, 254 - 46);
    
    free(node);
    free(file);
}

static void test_rrip_calc_len_symlink()
{
    IsoSymlink *link;
    Ecma119Node *node;
    Ecma119Image t;
    size_t sua_len = 0, ce_len = 0;
    
    memset(&t, 0, sizeof(Ecma119Image));
    t.input_charset = "UTF-8";
    t.output_charset = "UTF-8";
    
    link = malloc(sizeof(IsoSymlink));
    CU_ASSERT_PTR_NOT_NULL_FATAL(link);
    link->node.type = LIBISO_SYMLINK;
    
    node = malloc(sizeof(Ecma119Node));
    CU_ASSERT_PTR_NOT_NULL_FATAL(node);
    node->node = (IsoNode*)link;
    node->parent = (Ecma119Node*)0x55555555; /* just to make it not NULL */
    node->type = ECMA119_SYMLINK;
    
    /* Case 1. Name and dest fit in System Use field */
    link->node.name = "a small name.txt";
    link->dest = "/three/components";
    node->iso_name = "A_SMALL_.TXT";
    
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 0);
    CU_ASSERT_EQUAL(sua_len, 44 + (5 + 16) + (5 + 3*7) + 1 + 
                    (5 + 2 + (2+5) + (2+10)) );
    
    /* case 2. name + dest fits exactly */
    link->node.name = "this name will have 74 characters as it is the max "
                      "that fits in the SU.txt";
    link->dest = "./and/../a/./big/destination/with/10/components";
    node->iso_name = "THIS_NAM.TXT";
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 0);
    CU_ASSERT_EQUAL(sua_len, 254 - 46);

    /* case 3. name fits, dest is one byte larger to fit */
    /* 3.a extra byte in dest */
    link->node.name = "this name will have 74 characters as it is the max "
                      "that fits in the SU.txt";
    link->dest = "./and/../a/./big/destination/with/10/componentsk";
    node->iso_name = "THIS_NAM.TXT";
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 60);
    CU_ASSERT_EQUAL(sua_len, 44 + (5 + 74) + (5 + 3*7) + 1 + 28);
    
    /* 3.b extra byte in name */
    link->node.name = "this name will have 75 characters as it is the max "
                      "that fits in the SUx.txt";
    link->dest = "./and/../a/./big/destination/with/10/components";
    node->iso_name = "THIS_NAM.TXT";
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 59);
    CU_ASSERT_EQUAL(sua_len, 44 + (5 + 75) + (5 + 3*7) + 28);
    
    /* case 4. name seems to fit, but SL no, and when CE is added NM 
     * doesn't fit too */
    /* 4.a it just fits */ 
    link->node.name = "this name will have 105 characters as it is just the "
                      "max that fits in the SU once we add the CE entry.txt";
    link->dest = "./and/../a/./big/destination/with/10/components";
    node->iso_name = "THIS_NAM.TXT";
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 59);
    CU_ASSERT_EQUAL(sua_len, 254 - 46);
    
    /* 4.b it just fits, the the component ends in '/' */ 
    link->node.name = "this name will have 105 characters as it is just the "
                      "max that fits in the SU once we add the CE entry.txt";
    link->dest = "./and/../a/./big/destination/with/10/components/";
    node->iso_name = "THIS_NAM.TXT";
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 59);
    CU_ASSERT_EQUAL(sua_len, 254 - 46);
    
    /* 4.c extra char in name, that forces it to be divided */ 
    link->node.name = "this name will have 105 characters as it is just the "
                      "max that fits in the SU once we add the CE entryc.txt";
    link->dest = "./and/../a/./big/destination/with/10/components";
    node->iso_name = "THIS_NAM.TXT";
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 59 + 6);
    CU_ASSERT_EQUAL(sua_len, 254 - 46);
    
    /* 5 max destination length to fit in a single SL entry (250) */
    link->node.name = "this name will have 74 characters as it is the max "
                      "that fits in the SU.txt";
    link->dest = "./and/../a/./very/big/destination/with/10/components/that/"
                 "conforms/the/max/that/fits/in/a/single/SL/as/it/takes/"
                 "just/two/hundred/and/fifty/bytes/bytes/bytes/bytes/bytes"
                 "/bytes/bytes/bytes/bytes/bytes/bytes/../bytes";
    node->iso_name = "THIS_NAM.TXT";
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 255);
    CU_ASSERT_EQUAL(sua_len, 44 + (5 + 74) + (5 + 3*7) + 1 + 28);
    
    /* 6 min destination length to need two SL entries (251) */
    link->node.name = "this name will have 74 characters as it is the max "
                      "that fits in the SU.txt";
    link->dest = "./and/../a/./very/big/destination/with/10/components/that/"
                 "conforms/the/max/that/fits/in/a/single/SL/as/it/takes/"
                 "just/two/hundred/and/fifty/bytes/bytes/bytes/bytes/bytes"
                 "/bytes/bytes/bytes/bytes/bytes/bytes/../bytess";
    node->iso_name = "THIS_NAM.TXT";
    sua_len = rrip_calc_len(&t, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 261);
    CU_ASSERT_EQUAL(sua_len, 44 + (5 + 74) + (5 + 3*7) + 1 + 28);
    

}

static
void susp_info_free(struct susp_info *susp)
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

static
void test_rrip_get_susp_fields_file()
{
    IsoFile *file;
    Ecma119Node *node;
    int ret;
    struct susp_info susp;
    Ecma119Image t;
    uint8_t *entry;
    
    memset(&t, 0, sizeof(Ecma119Image));
    t.input_charset = "UTF-8";
    t.output_charset = "UTF-8";
    
    file = malloc(sizeof(IsoFile));
    CU_ASSERT_PTR_NOT_NULL_FATAL(file);
    file->msblock = 0;
    file->sort_weight = 0;
    file->stream = NULL; /* it is not needed here */
    file->node.type = LIBISO_FILE;
    file->node.mode = S_IFREG | 0555;
    file->node.uid = 235;
    file->node.gid = 654;
    file->node.mtime = 675757578;
    file->node.atime = 546462546;
    file->node.ctime = 323245342;
    
    node = malloc(sizeof(Ecma119Node));
    CU_ASSERT_PTR_NOT_NULL_FATAL(node);
    node->node = (IsoNode*)file;
    node->parent = (Ecma119Node*)0x55555555; /* just to make it not NULL */
    node->info.file = NULL; /* it is not needed here */ 
    node->type = ECMA119_FILE;
    node->nlink = 1;
    node->ino = 0x03447892;
    
    /* Case 1. Name fit in System Use field */
    file->node.name = "a small name.txt";
    node->iso_name = "A_SMALL_.TXT";
    
    memset(&susp, 0, sizeof(struct susp_info));
    ret = rrip_get_susp_fields(&t, node, 0, 255 - 46, &susp);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(susp.ce_len, 0);
    CU_ASSERT_EQUAL(susp.n_ce_susp_fields, 0);
    CU_ASSERT_EQUAL(susp.n_susp_fields, 3); /* PX + TF + NM */
    CU_ASSERT_EQUAL(susp.suf_len, 44 + (5 + 16) + (5 + 3*7) + 1);
    
    /* PX is the first entry */
    entry = susp.susp_fields[0];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'P');
    CU_ASSERT_EQUAL(entry[1], 'X');
    CU_ASSERT_EQUAL(entry[2], 44);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 4, 4), S_IFREG | 0555);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 8, 4), S_IFREG | 0555);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 12, 4), 1);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 16, 4), 1);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 20, 4), 235);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 24, 4), 235);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 28, 4), 654);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 32, 4), 654);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 36, 4), 0x03447892);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 40, 4), 0x03447892);
    
    /* TF is the second entry */
    entry = susp.susp_fields[1];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'T');
    CU_ASSERT_EQUAL(entry[1], 'F');
    CU_ASSERT_EQUAL(entry[2], 5 + 3*7);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 0x0E);
    CU_ASSERT_EQUAL(iso_datetime_read_7(entry + 5), 675757578);
    CU_ASSERT_EQUAL(iso_datetime_read_7(entry + 12), 546462546);
    CU_ASSERT_EQUAL(iso_datetime_read_7(entry + 19), 323245342);
    
    /* NM is the last entry */
    entry = susp.susp_fields[2];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'N');
    CU_ASSERT_EQUAL(entry[1], 'M');
    CU_ASSERT_EQUAL(entry[2], 5 + 16);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 0);
    CU_ASSERT_NSTRING_EQUAL(entry + 5, "a small name.txt", 16);
    
    susp_info_free(&susp);
    
    /* Case 2. Name fits exactly */
    file->node.name = "a big name, with 133 characters, that it is the max "
                      "that fits in System Use field of the directory record "
                      "PADPADPADADPADPADPADPAD.txt";
    node->iso_name = "A_BIG_NA.TXT";
    
    memset(&susp, 0, sizeof(struct susp_info));
    ret = rrip_get_susp_fields(&t, node, 0, 255 - 46, &susp);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(susp.ce_len, 0);
    CU_ASSERT_EQUAL(susp.n_ce_susp_fields, 0);
    CU_ASSERT_EQUAL(susp.suf_len, 254 - 46);
    
    CU_ASSERT_EQUAL(susp.n_susp_fields, 3); /* PX + TF + NM */
    
    /* NM is the last entry */
    entry = susp.susp_fields[2];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'N');
    CU_ASSERT_EQUAL(entry[1], 'M');
    CU_ASSERT_EQUAL(entry[2], 5 + 133);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 0);
    CU_ASSERT_NSTRING_EQUAL(entry + 5, "a big name, with 133 characters, that "
                      "it is the max that fits in System Use field of the "
                      "directory record PADPADPADADPADPADPADPAD.txt", 133);
    
    susp_info_free(&susp);
    
    /* case 3. A name just 1 character too big to fit in SUA */
    file->node.name = "a big name, with 133 characters, that it is the max "
                      "that fits in System Use field of the directory record "
                      "PADPADPADADPADPADPADPAD1.txt";
    node->iso_name = "A_BIG_NA.TXT";
    
    memset(&susp, 0, sizeof(struct susp_info));
    ret = rrip_get_susp_fields(&t, node, 0, 255 - 46, &susp);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(susp.ce_len, 28 + 5 + 1);
    CU_ASSERT_EQUAL(susp.suf_len, 254 - 46);
    
    CU_ASSERT_EQUAL(susp.n_susp_fields, 4); /* PX + TF + NM + CE */
    CU_ASSERT_EQUAL(susp.n_ce_susp_fields, 1); /* NM */
    
    /* test NM entry */
    entry = susp.susp_fields[2];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'N');
    CU_ASSERT_EQUAL(entry[1], 'M');
    CU_ASSERT_EQUAL(entry[2], 5 + 105);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 1); /* CONTINUE */
    CU_ASSERT_NSTRING_EQUAL(entry + 5, "a big name, with 133 characters, that "
                      "it is the max that fits in System Use field of the "
                      "directory record", 105);
    
    /* and CE entry */
    entry = susp.susp_fields[3];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'C');
    CU_ASSERT_EQUAL(entry[1], 'E');
    CU_ASSERT_EQUAL(entry[2], 28);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 4, 4), 0);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 8, 4), 0);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 12, 4), 0);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 16, 4), 0);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 20, 4), 34);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 24, 4), 34);
    
    /* and check Continuation area */
    entry = susp.ce_susp_fields[0];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'N');
    CU_ASSERT_EQUAL(entry[1], 'M');
    CU_ASSERT_EQUAL(entry[2], 5 + 29);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 0);
    CU_ASSERT_NSTRING_EQUAL(entry + 5, " PADPADPADADPADPADPADPAD1.txt", 29);
    
    susp_info_free(&susp);
    
    /* case 4. A 255 characters name */
    file->node.name = "a big name, with 255 characters, that it is the max "
        "that a POSIX filename can have. PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP"
        "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP"
        "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP";
    node->iso_name = "A_BIG_NA.TXT";
    
    memset(&susp, 0, sizeof(struct susp_info));
    susp.ce_block = 12;
    susp.ce_len = 456;
    ret = rrip_get_susp_fields(&t, node, 0, 255 - 46, &susp);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(susp.ce_len, 150 + 5 + 456);
    CU_ASSERT_EQUAL(susp.suf_len, 254 - 46);
    
    CU_ASSERT_EQUAL(susp.n_susp_fields, 4); /* PX + TF + NM + CE */
    CU_ASSERT_EQUAL(susp.n_ce_susp_fields, 1); /* NM */

    /* test NM entry */
    entry = susp.susp_fields[2];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'N');
    CU_ASSERT_EQUAL(entry[1], 'M');
    CU_ASSERT_EQUAL(entry[2], 5 + 105);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 1); /* CONTINUE */
    CU_ASSERT_NSTRING_EQUAL(entry + 5, "a big name, with 255 characters, that "
                            "it is the max that a POSIX filename can have. PPP"
                            "PPPPPPPPPPPPPPPPPP", 105);
    
    /* and CE entry */
    entry = susp.susp_fields[3];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'C');
    CU_ASSERT_EQUAL(entry[1], 'E');
    CU_ASSERT_EQUAL(entry[2], 28);
    CU_ASSERT_EQUAL(entry[3], 1);

    /* block, offset, size */
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 4, 4), 12);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 8, 4), 12);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 12, 4), 456);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 16, 4), 456);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 20, 4), 155);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 24, 4), 155);

    /* and check Continuation area */
    entry = susp.ce_susp_fields[0];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'N');
    CU_ASSERT_EQUAL(entry[1], 'M');
    CU_ASSERT_EQUAL(entry[2], 5 + 150);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 0);
    CU_ASSERT_NSTRING_EQUAL(entry + 5, "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP"
                            "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP"
                            "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP"
                            "PPPPPPPPPPPPPP", 150);
    
    susp_info_free(&susp);
    
    free(node);
    free(file);
}

static void test_rrip_get_susp_fields_symlink()
{
    IsoSymlink *link;
    Ecma119Node *node;
    Ecma119Image t;
    int ret;
    struct susp_info susp;
    uint8_t *entry;
    
    memset(&t, 0, sizeof(Ecma119Image));
    t.input_charset = "UTF-8";
    t.output_charset = "UTF-8";
    
    link = malloc(sizeof(IsoSymlink));
    CU_ASSERT_PTR_NOT_NULL_FATAL(link);
    link->node.type = LIBISO_SYMLINK;
    link->node.mode = S_IFREG | 0555;
    link->node.uid = 235;
    link->node.gid = 654;
    link->node.mtime = 675757578;
    link->node.atime = 546462546;
    link->node.ctime = 323245342;
    
    node = malloc(sizeof(Ecma119Node));
    CU_ASSERT_PTR_NOT_NULL_FATAL(node);
    node->node = (IsoNode*)link;
    node->parent = (Ecma119Node*)0x55555555; /* just to make it not NULL */
    node->type = ECMA119_SYMLINK;
    node->nlink = 1;
    node->ino = 0x03447892;
    
    /* Case 1. Name and dest fit in System Use field */
    link->node.name = "a small name.txt";
    link->dest = "/three/components";
    node->iso_name = "A_SMALL_.TXT";
    
    memset(&susp, 0, sizeof(struct susp_info));
    ret = rrip_get_susp_fields(&t, node, 0, 255 - 46, &susp);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(susp.ce_len, 0);
    CU_ASSERT_EQUAL(susp.n_ce_susp_fields, 0);
    CU_ASSERT_EQUAL(susp.n_susp_fields, 4); /* PX + TF + NM + SL */
    CU_ASSERT_EQUAL(susp.suf_len, 44 + (5 + 16) + (5 + 3*7) + 1 
                    + (5 + 2 + (2 + 5) + (2 + 10)));
    
    /* PX is the first entry */
    entry = susp.susp_fields[0];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'P');
    CU_ASSERT_EQUAL(entry[1], 'X');
    CU_ASSERT_EQUAL(entry[2], 44);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 4, 4), S_IFREG | 0555);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 8, 4), S_IFREG | 0555);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 12, 4), 1);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 16, 4), 1);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 20, 4), 235);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 24, 4), 235);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 28, 4), 654);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 32, 4), 654);
    CU_ASSERT_EQUAL(iso_read_lsb(entry + 36, 4), 0x03447892);
    CU_ASSERT_EQUAL(iso_read_msb(entry + 40, 4), 0x03447892);
    
    /* TF is the second entry */
    entry = susp.susp_fields[1];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'T');
    CU_ASSERT_EQUAL(entry[1], 'F');
    CU_ASSERT_EQUAL(entry[2], 5 + 3*7);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 0x0E);
    CU_ASSERT_EQUAL(iso_datetime_read_7(entry + 5), 675757578);
    CU_ASSERT_EQUAL(iso_datetime_read_7(entry + 12), 546462546);
    CU_ASSERT_EQUAL(iso_datetime_read_7(entry + 19), 323245342);
    
    /* NM is the 3rd entry */
    entry = susp.susp_fields[2];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'N');
    CU_ASSERT_EQUAL(entry[1], 'M');
    CU_ASSERT_EQUAL(entry[2], 5 + 16);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 0);
    CU_ASSERT_NSTRING_EQUAL(entry + 5, "a small name.txt", 16);

    /* SL is the last entry */
    entry = susp.susp_fields[3];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'S');
    CU_ASSERT_EQUAL(entry[1], 'L');
    CU_ASSERT_EQUAL(entry[2], 5 + 2 + (2 + 5) + (2 + 10));
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 0);
    
    /* first component */
    CU_ASSERT_EQUAL(entry[5], 0x8); /* root */
    CU_ASSERT_EQUAL(entry[6], 0);

    /* 2nd component */
    CU_ASSERT_EQUAL(entry[7], 0);
    CU_ASSERT_EQUAL(entry[8], 5);
    CU_ASSERT_NSTRING_EQUAL(entry + 9, "three", 5);
    
    /* 3rd component */
    CU_ASSERT_EQUAL(entry[14], 0);
    CU_ASSERT_EQUAL(entry[15], 10);
    CU_ASSERT_NSTRING_EQUAL(entry + 16, "components", 10);
    
    susp_info_free(&susp);
    
    /* case 2. name + dest fits exactly */
    link->node.name = "this name will have 74 characters as it is the max "
                      "that fits in the SU.txt";
    link->dest = "./and/../a/./big/destination/with/10/components";
    node->iso_name = "THIS_NAM.TXT";

    memset(&susp, 0, sizeof(struct susp_info));
    ret = rrip_get_susp_fields(&t, node, 0, 255 - 46, &susp);
    CU_ASSERT_EQUAL(ret, 1);
    CU_ASSERT_EQUAL(susp.ce_len, 0);
    CU_ASSERT_EQUAL(susp.n_ce_susp_fields, 0);
    CU_ASSERT_EQUAL(susp.n_susp_fields, 4); /* PX + TF + NM + SL */
    CU_ASSERT_EQUAL(susp.suf_len, 254 - 46);
    
    /* NM is the 3rd entry */
    entry = susp.susp_fields[2];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'N');
    CU_ASSERT_EQUAL(entry[1], 'M');
    CU_ASSERT_EQUAL(entry[2], 5 + 74);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 0);
    CU_ASSERT_NSTRING_EQUAL(entry + 5, "this name will have 74 characters as "
                            "it is the max that fits in the SU.txt", 74);

    /* SL is the last entry */
    entry = susp.susp_fields[3];
    CU_ASSERT_PTR_NOT_NULL(entry);
    CU_ASSERT_EQUAL(entry[0], 'S');
    CU_ASSERT_EQUAL(entry[1], 'L');
    CU_ASSERT_EQUAL(entry[2], 5 + 2 + 5 + 2 + 3 + 2 + 5 + 13 + 6 + 4 + 12);
    CU_ASSERT_EQUAL(entry[3], 1);
    CU_ASSERT_EQUAL(entry[4], 0);
    
    /* first component */
    CU_ASSERT_EQUAL(entry[5], 0x2); /* current */
    CU_ASSERT_EQUAL(entry[6], 0);

    /* 2nd component */
    CU_ASSERT_EQUAL(entry[7], 0);
    CU_ASSERT_EQUAL(entry[8], 3);
    CU_ASSERT_NSTRING_EQUAL(entry + 9, "and", 3);
    
    /* 3rd component */
    CU_ASSERT_EQUAL(entry[12], 0x4); /* parent */
    CU_ASSERT_EQUAL(entry[13], 0);

    /* 4th component */
    CU_ASSERT_EQUAL(entry[14], 0);
    CU_ASSERT_EQUAL(entry[15], 1);
    CU_ASSERT_EQUAL(entry[16], 'a');
    
    /* 5th component */
    CU_ASSERT_EQUAL(entry[17], 0x2); /* current */
    CU_ASSERT_EQUAL(entry[18], 0);

    /* 6th component */
    CU_ASSERT_EQUAL(entry[19], 0);
    CU_ASSERT_EQUAL(entry[20], 3);
    CU_ASSERT_NSTRING_EQUAL(entry + 21, "big", 3);

    /* 7th component */
    CU_ASSERT_EQUAL(entry[24], 0);
    CU_ASSERT_EQUAL(entry[25], 11);
    CU_ASSERT_NSTRING_EQUAL(entry + 26, "destination", 11);

    /* 8th component */
    CU_ASSERT_EQUAL(entry[37], 0);
    CU_ASSERT_EQUAL(entry[38], 4);
    CU_ASSERT_NSTRING_EQUAL(entry + 39, "with", 4);

    /* 9th component */
    CU_ASSERT_EQUAL(entry[43], 0);
    CU_ASSERT_EQUAL(entry[44], 2);
    CU_ASSERT_NSTRING_EQUAL(entry + 45, "10", 2);

    /* 10th component */
    CU_ASSERT_EQUAL(entry[47], 0);
    CU_ASSERT_EQUAL(entry[48], 10);
    CU_ASSERT_NSTRING_EQUAL(entry + 49, "components", 10);
    
    
    
    
    free(node);
    free(link);
}

void add_rockridge_suite() 
{
	CU_pSuite pSuite = CU_add_suite("RockRidge Suite", NULL, NULL);
	
	CU_add_test(pSuite, "rrip_calc_len(file)", test_rrip_calc_len_file);
    CU_add_test(pSuite, "rrip_calc_len(symlink)", test_rrip_calc_len_symlink);
    CU_add_test(pSuite, "rrip_get_susp_fields(file)", test_rrip_get_susp_fields_file);
    CU_add_test(pSuite, "rrip_get_susp_fields(symlink)", test_rrip_get_susp_fields_symlink);
}
