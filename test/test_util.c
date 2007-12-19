/*
 * Unit test for util.h
 * 
 * This test utiliy functions
 */
#include "test.h" 
#include "util.h"

#include <string.h>

static void test_div_up()
{
	CU_ASSERT_EQUAL( div_up(1, 2), 1 );
	CU_ASSERT_EQUAL( div_up(2, 2), 1 );
	CU_ASSERT_EQUAL( div_up(0, 2), 0 );
	CU_ASSERT_EQUAL( div_up(-1, 2), 0 );
	CU_ASSERT_EQUAL( div_up(3, 2), 2 );
}

static void test_round_up()
{
	CU_ASSERT_EQUAL( round_up(1, 2), 2 );
	CU_ASSERT_EQUAL( round_up(2, 2), 2 );
	CU_ASSERT_EQUAL( round_up(0, 2), 0 );
	CU_ASSERT_EQUAL( round_up(-1, 2), 0 );
	CU_ASSERT_EQUAL( round_up(3, 2), 4 );
	CU_ASSERT_EQUAL( round_up(15, 7), 21 );
	CU_ASSERT_EQUAL( round_up(13, 7), 14 );
	CU_ASSERT_EQUAL( round_up(14, 7), 14 );
}

static void test_iso_rbtree_insert()
{
    int res;
    IsoRBTree *tree;
    char *str1, *str2, *str3, *str4, *str5;
    void *str;
    
    res = iso_rbtree_new(strcmp, &tree);
    CU_ASSERT_EQUAL(res, 1);
    
    /* ok, insert one str  */
    str1 = "first str";
    res = iso_rbtree_insert(tree, str1, &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str1);
    
    str2 = "second str";
    res = iso_rbtree_insert(tree, str2, &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str2);
    
    /* an already inserted string */
    str3 = "second str";
    res = iso_rbtree_insert(tree, str3, &str);
    CU_ASSERT_EQUAL(res, 0);
    CU_ASSERT_PTR_EQUAL(str, str2);
    
    /* an already inserted string */
    str3 = "first str";
    res = iso_rbtree_insert(tree, str3, &str);
    CU_ASSERT_EQUAL(res, 0);
    CU_ASSERT_PTR_EQUAL(str, str1);
    
    str4 = "a string to be inserted first";
    res = iso_rbtree_insert(tree, str4, &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str4);
    
    str5 = "this to be inserted last";
    res = iso_rbtree_insert(tree, str5, &str);
    CU_ASSERT_EQUAL(res, 1);
    CU_ASSERT_PTR_EQUAL(str, str5);
    
    /*
     * TODO write a really good test to check all possible estrange
     * behaviors of a red-black tree
     */
    
    iso_rbtree_destroy(tree, NULL);
}

void add_util_suite() 
{
	CU_pSuite pSuite = CU_add_suite("UtilSuite", NULL, NULL);
	
	CU_add_test(pSuite, "div_up()", test_div_up);
	CU_add_test(pSuite, "round_up()", test_round_up);
    CU_add_test(pSuite, "iso_rbtree_insert()", test_iso_rbtree_insert);
}
