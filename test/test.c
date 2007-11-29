#include "test.h"

static void create_test_suite()
{
	add_node_suite();
} 

int main(int argc, char **argv)
{
	CU_pSuite pSuite = NULL;

	/* initialize the CUnit test registry */
	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();
	
	create_test_suite();

	/* Run all tests using the console interface */
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();
	return CU_get_error();
}
