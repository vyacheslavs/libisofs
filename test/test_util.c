/*
 * Unit test for util.h
 * 
 * This test utiliy functions
 */
#include "test.h" 
#include "util.h"

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

void add_util_suite() 
{
	CU_pSuite pSuite = CU_add_suite("UtilSuite", NULL, NULL);
	
	CU_add_test(pSuite, "div_up()", test_div_up);
	CU_add_test(pSuite, "round_up()", test_round_up);
}
