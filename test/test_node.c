/*
 * Unit test for node.h
 */

#include "libisofs.h"
#include "node.h"
#include "test.h"
#include "error.h"

#include <stdlib.h>

static
void test_iso_dir_add_node()
{
    int result;
    IsoDir *dir;
    IsoNode *node1, *node2, *node3, *node4, *node5;
    
    /* init dir with default values, not all field need to be initialized */
    dir = malloc(sizeof(IsoDir));
    dir->children = NULL;
    dir->nchildren = 0;
    
    /* 1st node to be added */
    node1 = calloc(1, sizeof(IsoNode));
    node1->name = "Node1";
    
    /* addition of node to an empty dir */
    result = iso_dir_add_node(dir, node1);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(dir->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(dir->children, node1);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_EQUAL(node1->parent, dir);
    
    /* addition of a node, to be inserted before */
    node2 = calloc(1, sizeof(IsoNode));
    node2->name = "A node to be added first";
    
    result = iso_dir_add_node(dir, node2);
    CU_ASSERT_EQUAL(result, 2);
    CU_ASSERT_EQUAL(dir->nchildren, 2);
    CU_ASSERT_PTR_EQUAL(dir->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node1);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_EQUAL(node2->parent, dir);
    
    /* addition of a node, to be inserted last */
    node3 = calloc(1, sizeof(IsoNode));
    node3->name = "This node will be inserted last";
    
    result = iso_dir_add_node(dir, node3);
    CU_ASSERT_EQUAL(result, 3);
    CU_ASSERT_EQUAL(dir->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(dir->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node1);
    CU_ASSERT_PTR_EQUAL(node1->next, node3);
    CU_ASSERT_PTR_NULL(node3->next);
    CU_ASSERT_PTR_EQUAL(node3->parent, dir);
    
    /* force some failures */
    result = iso_dir_add_node(NULL, node3);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    result = iso_dir_add_node(dir, NULL);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    
    result = iso_dir_add_node(dir, (IsoNode*)dir);
    CU_ASSERT_EQUAL(result, ISO_WRONG_ARG_VALUE);
    
    /* a node with same name */
    node4 = calloc(1, sizeof(IsoNode));
    node4->name = "This node will be inserted last";
    result = iso_dir_add_node(dir, node4);
    CU_ASSERT_EQUAL(result, ISO_NODE_NAME_NOT_UNIQUE);
    CU_ASSERT_EQUAL(dir->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(dir->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node1);
    CU_ASSERT_PTR_EQUAL(node1->next, node3);
    CU_ASSERT_PTR_NULL(node3->next);
    CU_ASSERT_PTR_NULL(node4->parent);
    
    /* a node already added to another dir should fail */
    node5 = calloc(1, sizeof(IsoNode));
    node5->name = "other node";
    node5->parent = (IsoDir*)node4;
    result = iso_dir_add_node(dir, node5);
    CU_ASSERT_EQUAL(result, ISO_NODE_ALREADY_ADDED);
    
    free(node1);
    free(node2);
    free(node3);
    free(node4);
    free(node5);
    free(dir);
}

static
void test_iso_dir_get_node()
{
    int result;
    IsoDir *dir;
    IsoNode *node1, *node2, *node3;
    IsoNode *node;
    
    /* init dir with default values, not all field need to be initialized */
    dir = malloc(sizeof(IsoDir));
    dir->children = NULL;
    dir->nchildren = 0;
    
    /* try to find a node in an empty dir */
    result = iso_dir_get_node(dir, "a inexistent name", &node);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_PTR_NULL(node);
    
    /* add a node */
    node1 = calloc(1, sizeof(IsoNode));
    node1->name = "Node1";
    result = iso_dir_add_node(dir, node1);
    
    /* try to find a node not existent */
    result = iso_dir_get_node(dir, "a inexistent name", &node);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_PTR_NULL(node);
    
    /* and an existing one */
    result = iso_dir_get_node(dir, "Node1", &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node1);
    
    /* add another node */
    node2 = calloc(1, sizeof(IsoNode));
    node2->name = "A node to be added first";
    result = iso_dir_add_node(dir, node2);
    
    /* try to find a node not existent */
    result = iso_dir_get_node(dir, "a inexistent name", &node);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_PTR_NULL(node);
    
    /* and the two existing */
    result = iso_dir_get_node(dir, "Node1", &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node1);
    result = iso_dir_get_node(dir, "A node to be added first", &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node2);
    
    /* insert another node */
    node3 = calloc(1, sizeof(IsoNode));
    node3->name = "This node will be inserted last";
    result = iso_dir_add_node(dir, node3);
    
    /* get again */
    result = iso_dir_get_node(dir, "a inexistent name", &node);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_PTR_NULL(node);
    result = iso_dir_get_node(dir, "This node will be inserted last", &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node3);
    
    /* force some failures */
    result = iso_dir_get_node(NULL, "asas", &node);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    result = iso_dir_get_node(dir, NULL, &node);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    result = iso_dir_get_node(dir, "asas", NULL);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    
    free(node1);
    free(node2);
    free(node3);
    free(dir);
}

void test_iso_dir_get_children()
{
    int result;
    IsoDirIter *iter;
    IsoDir *dir;
    IsoNode *node, *node1, *node2, *node3;
    
    /* init dir with default values, not all field need to be initialized */
    dir = malloc(sizeof(IsoDir));
    dir->children = NULL;
    dir->nchildren = 0;
    
    result = iso_dir_get_children(dir, &iter);
    CU_ASSERT_EQUAL(result, 1);
    
    /* item should have no items */
    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 0);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_PTR_NULL(node);
    iso_dir_iter_free(iter);
    
    /* 1st node to be added */
    node1 = calloc(1, sizeof(IsoNode));
    node1->name = "Node1";
    result = iso_dir_add_node(dir, node1);
    CU_ASSERT_EQUAL(dir->nchildren, 1);
    
    /* test iteration again */
    result = iso_dir_get_children(dir, &iter);
    CU_ASSERT_EQUAL(result, 1);
    
    /* iter should have a single item... */
    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 1);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node1);
    
    /* ...and no more */
    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 0);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_PTR_NULL(node);
    iso_dir_iter_free(iter);
    
    /* add another node */
    node2 = calloc(1, sizeof(IsoNode));
    node2->name = "A node to be added first";
    result = iso_dir_add_node(dir, node2);
    CU_ASSERT_EQUAL(result, 2);
    
    result = iso_dir_get_children(dir, &iter);
    CU_ASSERT_EQUAL(result, 1);
    
    /* iter should have two items... */
    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 1);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node2);
    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 1);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node1);
    
    /* ...and no more */
    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 0);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_PTR_NULL(node);
    iso_dir_iter_free(iter);
    
    /* addition of a 3rd node, to be inserted last */
    node3 = calloc(1, sizeof(IsoNode));
    node3->name = "This node will be inserted last";
    result = iso_dir_add_node(dir, node3);
    CU_ASSERT_EQUAL(result, 3);
    
    result = iso_dir_get_children(dir, &iter);
    CU_ASSERT_EQUAL(result, 1);
    
    /* iter should have 3 items... */
    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 1);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node2);
    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 1);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node1);
    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 1);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node3);
    
    /* ...and no more */
    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 0);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_PTR_NULL(node);
    iso_dir_iter_free(iter);
    
    free(node1);
    free(node2);
    free(node3);
    free(dir);
}

void add_node_suite()
{
	CU_pSuite pSuite = CU_add_suite("Node Test Suite", NULL, NULL);
	
    CU_add_test(pSuite, "iso_dir_add_node()", test_iso_dir_add_node);
    CU_add_test(pSuite, "iso_dir_get_node()", test_iso_dir_get_node);
    CU_add_test(pSuite, "iso_dir_get_children()", test_iso_dir_get_children);
}
