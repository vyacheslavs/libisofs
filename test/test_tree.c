/*
 * Unit test for node.h
 */

#include "libisofs.h"
#include "node.h"
#include "test.h"
#include "error.h"

#include <stdlib.h>

static
void test_iso_tree_add_new_dir()
{
    int result;
    IsoDir *root;
    IsoDir *node1, *node2, *node3, *node4;
    IsoImage *image;
    
    result = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(result, 1);
    root = iso_image_get_root(image);
    CU_ASSERT_PTR_NOT_NULL(root);
    
    result = iso_tree_add_new_dir(root, "Dir1", &node1);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(root->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(root->children, node1);
    CU_ASSERT_PTR_NULL(node1->node.next);
    CU_ASSERT_PTR_EQUAL(node1->node.parent, root);
    CU_ASSERT_EQUAL(node1->node.type, LIBISO_DIR);
    CU_ASSERT_STRING_EQUAL(node1->node.name, "Dir1");
    
    /* creation of a second dir, to be inserted before */
    result = iso_tree_add_new_dir(root, "A node to be added first", &node2);
    CU_ASSERT_EQUAL(result, 2);
    CU_ASSERT_EQUAL(root->nchildren, 2);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->node.next, node1);
    CU_ASSERT_PTR_NULL(node1->node.next);
    CU_ASSERT_PTR_EQUAL(node2->node.parent, root);
    CU_ASSERT_EQUAL(node2->node.type, LIBISO_DIR);
    CU_ASSERT_STRING_EQUAL(node2->node.name, "A node to be added first");
    
    /* creation of a 3rd node, to be inserted last */
    result = iso_tree_add_new_dir(root, "This node will be inserted last", &node3);
    CU_ASSERT_EQUAL(result, 3);
    CU_ASSERT_EQUAL(root->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->node.next, node1);
    CU_ASSERT_PTR_EQUAL(node1->node.next, node3);
    CU_ASSERT_PTR_NULL(node3->node.next);
    CU_ASSERT_PTR_EQUAL(node3->node.parent, root);
    CU_ASSERT_EQUAL(node3->node.type, LIBISO_DIR);
    CU_ASSERT_STRING_EQUAL(node3->node.name, "This node will be inserted last");
    
    /* force some failures */
    result = iso_tree_add_new_dir(NULL, "dsadas", &node4);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    result = iso_tree_add_new_dir(root, NULL, &node4);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    
    /* try to insert a new dir with same name */
    result = iso_tree_add_new_dir(root, "This node will be inserted last", &node4);
    CU_ASSERT_EQUAL(result, ISO_NODE_NAME_NOT_UNIQUE);
    CU_ASSERT_EQUAL(root->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->node.next, node1);
    CU_ASSERT_PTR_EQUAL(node1->node.next, node3);
    CU_ASSERT_PTR_NULL(node3->node.next);
    CU_ASSERT_PTR_NULL(node4);
    
    /* but pointer to new dir can be null */
    result = iso_tree_add_new_dir(root, "Another node", NULL);
    CU_ASSERT_EQUAL(result, 4);
    CU_ASSERT_EQUAL(root->nchildren, 4);
    CU_ASSERT_PTR_EQUAL(node2->node.next->next, node1);
    CU_ASSERT_STRING_EQUAL(node2->node.next->name, "Another node");
    
    iso_image_unref(image);
}

static
void test_iso_tree_add_new_symlink()
{
    int result;
    IsoDir *root;
    IsoSymlink *node1, *node2, *node3, *node4;
    IsoImage *image;
    
    result = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(result, 1);
    root = iso_image_get_root(image);
    CU_ASSERT_PTR_NOT_NULL(root);
    
    result = iso_tree_add_new_symlink(root, "Link1", "/path/to/dest", &node1);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(root->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(root->children, node1);
    CU_ASSERT_PTR_NULL(node1->node.next);
    CU_ASSERT_PTR_EQUAL(node1->node.parent, root);
    CU_ASSERT_EQUAL(node1->node.type, LIBISO_SYMLINK);
    CU_ASSERT_STRING_EQUAL(node1->node.name, "Link1");
    CU_ASSERT_STRING_EQUAL(node1->dest, "/path/to/dest");
    
    /* creation of a second link, to be inserted before */
    result = iso_tree_add_new_symlink(root, "A node to be added first", "/home/me", &node2);
    CU_ASSERT_EQUAL(result, 2);
    CU_ASSERT_EQUAL(root->nchildren, 2);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->node.next, node1);
    CU_ASSERT_PTR_NULL(node1->node.next);
    CU_ASSERT_PTR_EQUAL(node2->node.parent, root);
    CU_ASSERT_EQUAL(node2->node.type, LIBISO_SYMLINK);
    CU_ASSERT_STRING_EQUAL(node2->node.name, "A node to be added first");
    CU_ASSERT_STRING_EQUAL(node2->dest, "/home/me");
    
    /* creation of a 3rd node, to be inserted last */
    result = iso_tree_add_new_symlink(root, "This node will be inserted last", 
                                      "/path/to/dest", &node3);
    CU_ASSERT_EQUAL(result, 3);
    CU_ASSERT_EQUAL(root->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->node.next, node1);
    CU_ASSERT_PTR_EQUAL(node1->node.next, node3);
    CU_ASSERT_PTR_NULL(node3->node.next);
    CU_ASSERT_PTR_EQUAL(node3->node.parent, root);
    CU_ASSERT_EQUAL(node3->node.type, LIBISO_SYMLINK);
    CU_ASSERT_STRING_EQUAL(node3->node.name, "This node will be inserted last");
    CU_ASSERT_STRING_EQUAL(node3->dest, "/path/to/dest");
    
    /* force some failures */
    result = iso_tree_add_new_symlink(NULL, "dsadas", "/path/to/dest", &node4);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    result = iso_tree_add_new_symlink(root, NULL, "/path/to/dest", &node4);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    result = iso_tree_add_new_symlink(root, "dsadas", NULL, &node4);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    
    /* try to insert a new link with same name */
    result = iso_tree_add_new_symlink(root, "This node will be inserted last", "/", &node4);
    CU_ASSERT_EQUAL(result, ISO_NODE_NAME_NOT_UNIQUE);
    CU_ASSERT_EQUAL(root->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->node.next, node1);
    CU_ASSERT_PTR_EQUAL(node1->node.next, node3);
    CU_ASSERT_PTR_NULL(node3->node.next);
    CU_ASSERT_PTR_NULL(node4);
    
    /* but pointer to new link can be null */
    result = iso_tree_add_new_symlink(root, "Another node", ".", NULL);
    CU_ASSERT_EQUAL(result, 4);
    CU_ASSERT_EQUAL(root->nchildren, 4);
    CU_ASSERT_PTR_EQUAL(node2->node.next->next, node1);
    CU_ASSERT_EQUAL(node2->node.next->type, LIBISO_SYMLINK);
    CU_ASSERT_STRING_EQUAL(((IsoSymlink*)(node2->node.next))->dest, ".");
    CU_ASSERT_STRING_EQUAL(node2->node.next->name, "Another node");
    
    iso_image_unref(image);
}

static
void test_iso_tree_add_new_special()
{
    int result;
    IsoDir *root;
    IsoSpecial *node1, *node2, *node3, *node4;
    IsoImage *image;
    
    result = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(result, 1);
    root = iso_image_get_root(image);
    CU_ASSERT_PTR_NOT_NULL(root);
    
    result = iso_tree_add_new_special(root, "Special1", S_IFSOCK | 0644, 0, &node1);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(root->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(root->children, node1);
    CU_ASSERT_PTR_NULL(node1->node.next);
    CU_ASSERT_PTR_EQUAL(node1->node.parent, root);
    CU_ASSERT_EQUAL(node1->node.type, LIBISO_SPECIAL);
    CU_ASSERT_STRING_EQUAL(node1->node.name, "Special1");
    CU_ASSERT_EQUAL(node1->dev, 0);
    CU_ASSERT_EQUAL(node1->node.mode, S_IFSOCK | 0644);
    
    /* creation of a block dev, to be inserted before */
    result = iso_tree_add_new_special(root, "A node to be added first", S_IFBLK | 0640, 34, &node2);
    CU_ASSERT_EQUAL(result, 2);
    CU_ASSERT_EQUAL(root->nchildren, 2);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->node.next, node1);
    CU_ASSERT_PTR_NULL(node1->node.next);
    CU_ASSERT_PTR_EQUAL(node2->node.parent, root);
    CU_ASSERT_EQUAL(node2->node.type, LIBISO_SPECIAL);
    CU_ASSERT_STRING_EQUAL(node2->node.name, "A node to be added first");
    CU_ASSERT_EQUAL(node2->dev, 34);
    CU_ASSERT_EQUAL(node2->node.mode, S_IFBLK | 0640);
    
    /* creation of a 3rd node, to be inserted last */
    result = iso_tree_add_new_special(root, "This node will be inserted last", 
                                      S_IFCHR | 0440, 345, &node3);
    CU_ASSERT_EQUAL(result, 3);
    CU_ASSERT_EQUAL(root->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->node.next, node1);
    CU_ASSERT_PTR_EQUAL(node1->node.next, node3);
    CU_ASSERT_PTR_NULL(node3->node.next);
    CU_ASSERT_PTR_EQUAL(node3->node.parent, root);
    CU_ASSERT_EQUAL(node3->node.type, LIBISO_SPECIAL);
    CU_ASSERT_STRING_EQUAL(node3->node.name, "This node will be inserted last");
    CU_ASSERT_EQUAL(node3->dev, 345);
    CU_ASSERT_EQUAL(node3->node.mode, S_IFCHR | 0440);
    
    /* force some failures */
    result = iso_tree_add_new_special(NULL, "dsadas",  S_IFBLK | 0440, 345, &node4);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    result = iso_tree_add_new_special(root, NULL, S_IFBLK | 0440, 345, &node4);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    result = iso_tree_add_new_special(root, "dsadas", S_IFDIR | 0666, 0, &node4);
    CU_ASSERT_EQUAL(result, ISO_WRONG_ARG_VALUE);
    result = iso_tree_add_new_special(root, "dsadas", S_IFREG | 0666, 0, &node4);
    CU_ASSERT_EQUAL(result, ISO_WRONG_ARG_VALUE);
    result = iso_tree_add_new_special(root, "dsadas", S_IFLNK | 0666, 0, &node4);
    CU_ASSERT_EQUAL(result, ISO_WRONG_ARG_VALUE);
    
    /* try to insert a new special file with same name */
    result = iso_tree_add_new_special(root, "This node will be inserted last", S_IFIFO | 0666, 0, &node4);
    CU_ASSERT_EQUAL(result, ISO_NODE_NAME_NOT_UNIQUE);
    CU_ASSERT_EQUAL(root->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->node.next, node1);
    CU_ASSERT_PTR_EQUAL(node1->node.next, node3);
    CU_ASSERT_PTR_NULL(node3->node.next);
    CU_ASSERT_PTR_NULL(node4);
    
    /* but pointer to new special can be null */
    result = iso_tree_add_new_special(root, "Another node", S_IFIFO | 0666, 0, NULL);
    CU_ASSERT_EQUAL(result, 4);
    CU_ASSERT_EQUAL(root->nchildren, 4);
    CU_ASSERT_PTR_EQUAL(node2->node.next->next, node1);
    CU_ASSERT_EQUAL(node2->node.next->type, LIBISO_SPECIAL);
    CU_ASSERT_EQUAL(((IsoSpecial*)(node2->node.next))->dev, 0);
    CU_ASSERT_EQUAL(node2->node.next->mode, S_IFIFO | 0666);
    CU_ASSERT_STRING_EQUAL(node2->node.next->name, "Another node");
    
    iso_image_unref(image);
}

void add_tree_suite()
{
	CU_pSuite pSuite = CU_add_suite("Iso Tree Suite", NULL, NULL);
   
    CU_add_test(pSuite, "iso_tree_add_new_dir()", test_iso_tree_add_new_dir);
    CU_add_test(pSuite, "iso_tree_add_new_symlink()", test_iso_tree_add_new_symlink);
    CU_add_test(pSuite, "iso_tree_add_new_special()", test_iso_tree_add_new_special);
}
