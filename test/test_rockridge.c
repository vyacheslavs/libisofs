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
    size_t sua_len = 0, ce_len = 0;
    
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
    
    sua_len = rrip_calc_len(NULL, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 0);
    CU_ASSERT_EQUAL(sua_len, 44 + (5 + 16) + (5 + 3*7) + 1);
    
    /* Case 2. Name fits exactly */
    file->node.name = "a big name, with 133 characters, that it is the max "
                      "that fits in System Use field of the directory record "
                      "PADPADPADADPADPADPADPAD.txt";
    node->iso_name = "A_BIG_NA.TXT";
    
    sua_len = rrip_calc_len(NULL, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 0);
    /* note that 254 is the max length of a directory record, as it needs to
     * be an even number */
    CU_ASSERT_EQUAL(sua_len, 254 - 46);
    
    /* case 3. A name just 1 character too big to fit in SUA */
    file->node.name = "a big name, with 133 characters, that it is the max "
                      "that fits in System Use field of the directory record "
                      "PADPADPADADPADPADPADPAD1.txt";
    node->iso_name = "A_BIG_NA.TXT";
    
    sua_len = rrip_calc_len(NULL, node, 0, 255 - 46, &ce_len);
    /* 28 (the chars moved to include the CE entry) + 5 (header of NM in CE) +
     * 1 (the char that originally didn't fit) */
    CU_ASSERT_EQUAL(ce_len, 28 + 5 + 1);
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
    size_t sua_len = 0, ce_len = 0;
    
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
    
    sua_len = rrip_calc_len(NULL, node, 0, 255 - 46, &ce_len);
    CU_ASSERT_EQUAL(ce_len, 0);
    CU_ASSERT_EQUAL(sua_len, 44 + (5 + 16) + (5 + 3*7) + 1 + 
                    (5 + 2 + (2+5) + (2+10)) );
    
    /* case 2. name + dest fits exactly */
    //TODO
}

void add_rockridge_suite() 
{
	CU_pSuite pSuite = CU_add_suite("RockRidge Suite", NULL, NULL);
	
	CU_add_test(pSuite, "rrip_calc_len(file)", test_rrip_calc_len_file);
    CU_add_test(pSuite, "rrip_calc_len(symlink)", test_rrip_calc_len_symlink);
}
