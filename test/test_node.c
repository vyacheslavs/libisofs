/*
 * Unit test for node.h
 */

#include "libisofs.h"
#include "node.h"
#include "test.h"

#include <stdlib.h>

static
void test_iso_node_new_root()
{
    int ret;
    IsoDir *dir;
    
    ret = iso_node_new_root(&dir);
    CU_ASSERT_EQUAL(ret, ISO_SUCCESS);
    
    CU_ASSERT_EQUAL(dir->node.refcount, 1);
    CU_ASSERT_EQUAL(dir->node.type, LIBISO_DIR);
    CU_ASSERT_EQUAL(dir->node.mode, S_IFDIR | 0555);
    CU_ASSERT_EQUAL(dir->node.uid, 0);
    CU_ASSERT_EQUAL(dir->node.gid, 0);
    CU_ASSERT_PTR_NULL(dir->node.name);
    CU_ASSERT_EQUAL(dir->node.hidden, 0);
    CU_ASSERT_PTR_EQUAL(dir->node.parent, dir);
    CU_ASSERT_PTR_NULL(dir->node.next);
    CU_ASSERT_EQUAL(dir->nchildren, 0);
    CU_ASSERT_PTR_NULL(dir->children);
    
    iso_node_unref((IsoNode*)dir);
}

static
void test_iso_node_new_dir()
{
    int ret;
    IsoDir *dir;
    char *name;

    name = strdup("name1");
    ret = iso_node_new_dir(name, &dir);
    CU_ASSERT_EQUAL(ret, ISO_SUCCESS);
    CU_ASSERT_EQUAL(dir->node.refcount, 1);
    CU_ASSERT_EQUAL(dir->node.type, LIBISO_DIR);
    CU_ASSERT_EQUAL(dir->node.mode, S_IFDIR);
    CU_ASSERT_EQUAL(dir->node.uid, 0);
    CU_ASSERT_EQUAL(dir->node.gid, 0);
    CU_ASSERT_EQUAL(dir->node.atime, 0);
    CU_ASSERT_EQUAL(dir->node.mtime, 0);
    CU_ASSERT_EQUAL(dir->node.ctime, 0);
    CU_ASSERT_STRING_EQUAL(dir->node.name, "name1");
    CU_ASSERT_EQUAL(dir->node.hidden, 0);
    CU_ASSERT_PTR_NULL(dir->node.parent);
    CU_ASSERT_PTR_NULL(dir->node.next);
    CU_ASSERT_EQUAL(dir->nchildren, 0);
    CU_ASSERT_PTR_NULL(dir->children);
    
    iso_node_unref((IsoNode*)dir);
    
    /* try with invalid names */
    ret = iso_node_new_dir("H/DHS/s", &dir);
    CU_ASSERT_EQUAL(ret, ISO_WRONG_ARG_VALUE);
    ret = iso_node_new_dir(".", &dir);
    CU_ASSERT_EQUAL(ret, ISO_WRONG_ARG_VALUE);
    ret = iso_node_new_dir("..", &dir);
    CU_ASSERT_EQUAL(ret, ISO_WRONG_ARG_VALUE);
    ret = iso_node_new_dir(NULL, &dir);
    CU_ASSERT_EQUAL(ret, ISO_NULL_POINTER);
}

static
void test_iso_node_new_symlink()
{
    int ret;
    IsoSymlink *link;
    char *name, *dest;

    name = strdup("name1");
    dest = strdup("/home");
    ret = iso_node_new_symlink(name, dest, &link);
    CU_ASSERT_EQUAL(ret, ISO_SUCCESS);
    CU_ASSERT_EQUAL(link->node.refcount, 1);
    CU_ASSERT_EQUAL(link->node.type, LIBISO_SYMLINK);
    CU_ASSERT_EQUAL(link->node.mode, S_IFLNK);
    CU_ASSERT_EQUAL(link->node.uid, 0);
    CU_ASSERT_EQUAL(link->node.gid, 0);
    CU_ASSERT_EQUAL(link->node.atime, 0);
    CU_ASSERT_EQUAL(link->node.mtime, 0);
    CU_ASSERT_EQUAL(link->node.ctime, 0);
    CU_ASSERT_STRING_EQUAL(link->node.name, "name1");
    CU_ASSERT_EQUAL(link->node.hidden, 0);
    CU_ASSERT_PTR_NULL(link->node.parent);
    CU_ASSERT_PTR_NULL(link->node.next);
    CU_ASSERT_STRING_EQUAL(link->dest, "/home");
    
    iso_node_unref((IsoNode*)link);
    
    /* try with invalid names */
    ret = iso_node_new_symlink("H/DHS/s", "/home", &link);
    CU_ASSERT_EQUAL(ret, ISO_WRONG_ARG_VALUE);
    ret = iso_node_new_symlink(".", "/home", &link);
    CU_ASSERT_EQUAL(ret, ISO_WRONG_ARG_VALUE);
    ret = iso_node_new_symlink("..", "/home", &link);
    CU_ASSERT_EQUAL(ret, ISO_WRONG_ARG_VALUE);
}

static
void test_iso_node_set_permissions()
{
    IsoNode *node;
    node = malloc(sizeof(IsoNode));
    
    node->mode = S_IFDIR | 0777;
    
    /* set permissions propertly */
    iso_node_set_permissions(node, 0555);
    CU_ASSERT_EQUAL(node->mode, S_IFDIR | 0555);
    iso_node_set_permissions(node, 0640);
    CU_ASSERT_EQUAL(node->mode, S_IFDIR | 0640);
    
    /* try to change file type via this call */
    iso_node_set_permissions(node, S_IFBLK | 0440);
    CU_ASSERT_EQUAL(node->mode, S_IFDIR | 0440);
    
    free(node);
}

static
void test_iso_node_get_permissions()
{
    IsoNode *node;
    mode_t mode;
    
    node = malloc(sizeof(IsoNode));
    node->mode = S_IFDIR | 0777;
    
    mode = iso_node_get_permissions(node);
    CU_ASSERT_EQUAL(mode, 0777);
    
    iso_node_set_permissions(node, 0640);
    mode = iso_node_get_permissions(node);
    CU_ASSERT_EQUAL(mode, 0640);
    
    iso_node_set_permissions(node, S_IFBLK | 0440);
    mode = iso_node_get_permissions(node);
    CU_ASSERT_EQUAL(mode, 0440);
   
    free(node);
}

static
void test_iso_node_get_mode()
{
    IsoNode *node;
    mode_t mode;
    
    node = malloc(sizeof(IsoNode));
    node->mode = S_IFDIR | 0777;
    
    mode = iso_node_get_mode(node);
    CU_ASSERT_EQUAL(mode, S_IFDIR | 0777);
    
    iso_node_set_permissions(node, 0640);
    mode = iso_node_get_mode(node);
    CU_ASSERT_EQUAL(mode, S_IFDIR | 0640);
    
    iso_node_set_permissions(node, S_IFBLK | 0440);
    mode = iso_node_get_mode(node);
    CU_ASSERT_EQUAL(mode, S_IFDIR | 0440);
   
    free(node);
}

static
void test_iso_node_set_uid()
{
    IsoNode *node;
    node = malloc(sizeof(IsoNode));
    
    node->uid = 0;
    
    iso_node_set_uid(node, 23);
    CU_ASSERT_EQUAL(node->uid, 23);
    iso_node_set_uid(node, 0);
    CU_ASSERT_EQUAL(node->uid, 0);
    
    free(node);
}

static
void test_iso_node_get_uid()
{
    IsoNode *node;
    uid_t uid;
    
    node = malloc(sizeof(IsoNode));
    node->uid = 0;
    
    uid = iso_node_get_uid(node);
    CU_ASSERT_EQUAL(uid, 0);
    
    iso_node_set_uid(node, 25);
    uid = iso_node_get_uid(node);
    CU_ASSERT_EQUAL(uid, 25);
   
    free(node);
}

static
void test_iso_node_set_gid()
{
    IsoNode *node;
    node = malloc(sizeof(IsoNode));
    
    node->gid = 0;
    
    iso_node_set_gid(node, 23);
    CU_ASSERT_EQUAL(node->gid, 23);
    iso_node_set_gid(node, 0);
    CU_ASSERT_EQUAL(node->gid, 0);
    
    free(node);
}

static
void test_iso_node_get_gid()
{
    IsoNode *node;
    gid_t gid;
    
    node = malloc(sizeof(IsoNode));
    node->gid = 0;
    
    gid = iso_node_get_gid(node);
    CU_ASSERT_EQUAL(gid, 0);
    
    iso_node_set_gid(node, 25);
    gid = iso_node_get_gid(node);
    CU_ASSERT_EQUAL(gid, 25);
   
    free(node);
}

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
    result = iso_dir_add_node(dir, node1, 0);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(dir->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(dir->children, node1);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_EQUAL(node1->parent, dir);
    
    /* addition of a node, to be inserted before */
    node2 = calloc(1, sizeof(IsoNode));
    node2->name = "A node to be added first";
    
    result = iso_dir_add_node(dir, node2, 0);
    CU_ASSERT_EQUAL(result, 2);
    CU_ASSERT_EQUAL(dir->nchildren, 2);
    CU_ASSERT_PTR_EQUAL(dir->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node1);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_EQUAL(node2->parent, dir);
    
    /* addition of a node, to be inserted last */
    node3 = calloc(1, sizeof(IsoNode));
    node3->name = "This node will be inserted last";
    
    result = iso_dir_add_node(dir, node3, 0);
    CU_ASSERT_EQUAL(result, 3);
    CU_ASSERT_EQUAL(dir->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(dir->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node1);
    CU_ASSERT_PTR_EQUAL(node1->next, node3);
    CU_ASSERT_PTR_NULL(node3->next);
    CU_ASSERT_PTR_EQUAL(node3->parent, dir);
    
    /* force some failures */
    result = iso_dir_add_node(NULL, node3, 0);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    result = iso_dir_add_node(dir, NULL, 0);
    CU_ASSERT_EQUAL(result, ISO_NULL_POINTER);
    
    result = iso_dir_add_node(dir, (IsoNode*)dir, 0);
    CU_ASSERT_EQUAL(result, ISO_WRONG_ARG_VALUE);
    
    /* a node with same name */
    node4 = calloc(1, sizeof(IsoNode));
    node4->name = "This node will be inserted last";
    result = iso_dir_add_node(dir, node4, 0);
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
    result = iso_dir_add_node(dir, node5, 0);
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
    result = iso_dir_add_node(dir, node1, 0);
    
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
    result = iso_dir_add_node(dir, node2, 0);
    
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
    result = iso_dir_add_node(dir, node3, 0);
    
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
    
    /* and try with null node */
    result = iso_dir_get_node(dir, "asas", NULL);
    CU_ASSERT_EQUAL(result, 0);
    result = iso_dir_get_node(dir, "This node will be inserted last", NULL);
    CU_ASSERT_EQUAL(result, 1);
    
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
    result = iso_dir_add_node(dir, node1, 0);
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
    result = iso_dir_add_node(dir, node2, 0);
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
    result = iso_dir_add_node(dir, node3, 0);
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

void test_iso_dir_iter_take()
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
    
    /* remove on empty dir! */
    result = iso_dir_iter_take(iter);
    CU_ASSERT_TRUE(result < 0); /* should fail */
    
    iso_dir_iter_free(iter);
    
    /* 1st node to be added */
    node1 = calloc(1, sizeof(IsoNode));
    node1->name = "Node1";
    result = iso_dir_add_node(dir, node1, 0);
    CU_ASSERT_EQUAL(dir->nchildren, 1);
    
    /* test iteration again */
    result = iso_dir_get_children(dir, &iter);
    CU_ASSERT_EQUAL(result, 1);    
    
    /* remove before iso_dir_iter_next() */
    result = iso_dir_iter_take(iter);
    CU_ASSERT_TRUE(result < 0); /* should fail */
    
    result = iso_dir_iter_next(iter, &node);
    
    /* this should remove the child */
    result = iso_dir_iter_take(iter);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(dir->nchildren, 0);
    CU_ASSERT_PTR_NULL(dir->children);

    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 0);
    
    iso_dir_iter_free(iter);

    /* add two node */
    result = iso_dir_add_node(dir, node1, 0);
    CU_ASSERT_EQUAL(dir->nchildren, 1);
    
    node2 = calloc(1, sizeof(IsoNode));
    node2->name = "A node to be added first";
    result = iso_dir_add_node(dir, node2, 0);
    CU_ASSERT_EQUAL(result, 2);
    
    result = iso_dir_get_children(dir, &iter);
    CU_ASSERT_EQUAL(result, 1);

    /* remove before iso_dir_iter_next() */
    result = iso_dir_iter_take(iter);
    CU_ASSERT_TRUE(result < 0); /* should fail */
    
    /* iter should have two items... */
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node2);
    
    /* remove node 2 */
    result = iso_dir_iter_take(iter);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(dir->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(dir->children, node1);
    
    /* next should still work */
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
    
    /* now remove only last child */
    result = iso_dir_add_node(dir, node2, 0);
    CU_ASSERT_EQUAL(result, 2);

    result = iso_dir_get_children(dir, &iter);
    CU_ASSERT_EQUAL(result, 1);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node2);
    
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node1);
    
    /* take last child */
    result = iso_dir_iter_take(iter);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(dir->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(dir->children, node2);

    result = iso_dir_iter_has_next(iter);
    CU_ASSERT_EQUAL(result, 0);
    
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_PTR_NULL(node);
    iso_dir_iter_free(iter);
    
    /* Ok, now another situation. Modification of dir during iteration */
    result = iso_dir_add_node(dir, node1, 0);
    CU_ASSERT_EQUAL(result, 2);

    result = iso_dir_get_children(dir, &iter);
    CU_ASSERT_EQUAL(result, 1);
    result = iso_dir_iter_next(iter, &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node2);
    
    /* returned dir is node2, it should be the node taken next, but
     * let's insert a node after node2 and before node1 */
    node3 = calloc(1, sizeof(IsoNode));
    node3->name = "A node to be added second";
    result = iso_dir_add_node(dir, node3, 0);
    CU_ASSERT_EQUAL(dir->nchildren, 3);

    /* is the node 2 the removed one? */
    result = iso_dir_iter_take(iter);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(dir->nchildren, 2);
    CU_ASSERT_PTR_EQUAL(dir->children, node3);
    CU_ASSERT_PTR_EQUAL(node3->next, node1);

    iso_dir_iter_free(iter);
    
    free(node1);
    free(node2);
    free(node3);
    free(dir);
}

void test_iso_node_take()
{
    int result;
    IsoDir *dir;
    IsoNode *node1, *node2, *node3;
    
    /* init dir with default values, not all field need to be initialized */
    dir = malloc(sizeof(IsoDir));
    dir->children = NULL;
    dir->nchildren = 0;
    
    /* 1st node to be added */
    node1 = calloc(1, sizeof(IsoNode));
    node1->name = "Node1";
    
    /* addition of node to an empty dir */
    result = iso_dir_add_node(dir, node1, 0);
    CU_ASSERT_EQUAL(result, 1);
    
    /* and take it */
    result = iso_node_take(node1);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(dir->nchildren, 0);
    CU_ASSERT_PTR_NULL(dir->children);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_NULL(node1->parent);
    
    /* insert it again */
    result = iso_dir_add_node(dir, node1, 0);
    CU_ASSERT_EQUAL(result, 1);
    
    /* addition of a 2nd node, to be inserted before */
    node2 = calloc(1, sizeof(IsoNode));
    node2->name = "A node to be added first";
    result = iso_dir_add_node(dir, node2, 0);
    CU_ASSERT_EQUAL(result, 2);
    
    /* take first child */
    result = iso_node_take(node2);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(dir->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(dir->children, node1);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_EQUAL(node1->parent, dir);
    CU_ASSERT_PTR_NULL(node2->next);
    CU_ASSERT_PTR_NULL(node2->parent);
    
    /* insert node 2 again */
    result = iso_dir_add_node(dir, node2, 0);
    CU_ASSERT_EQUAL(result, 2);
    
    /* now take last child */
    result = iso_node_take(node1);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(dir->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(dir->children, node2);
    CU_ASSERT_PTR_NULL(node2->next);
    CU_ASSERT_PTR_EQUAL(node2->parent, dir);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_NULL(node1->parent);
    
    /* insert again node1... */
    result = iso_dir_add_node(dir, node1, 0);
    CU_ASSERT_EQUAL(result, 2);
    
    /* ...and a 3rd child, to be inserted last */
    node3 = calloc(1, sizeof(IsoNode));
    node3->name = "This node will be inserted last";
    result = iso_dir_add_node(dir, node3, 0);
    CU_ASSERT_EQUAL(result, 3);
    
    /* and take the node in the middle */
    result = iso_node_take(node1);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(dir->nchildren, 2);
    CU_ASSERT_PTR_EQUAL(dir->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node3);
    CU_ASSERT_PTR_EQUAL(node2->parent, dir);
    CU_ASSERT_PTR_NULL(node3->next);
    CU_ASSERT_PTR_EQUAL(node3->parent, dir);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_NULL(node1->parent);
    
    free(node1);
    free(node2);
    free(node3);
    free(dir);
}

static
void test_iso_node_set_name()
{
    int result;
    IsoDir *dir;
    IsoNode *node1, *node2;
    
    /* init dir with default values, not all field need to be initialized */
    dir = malloc(sizeof(IsoDir));
    dir->children = NULL;
    dir->nchildren = 0;
    
    /* cretae a node */
    node1 = calloc(1, sizeof(IsoNode));
    node1->name = strdup("Node1");
    
    /* check name change */
    result = iso_node_set_name(node1, "New name");
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_STRING_EQUAL(node1->name, "New name");
    
    /* add node dir */
    result = iso_dir_add_node(dir, node1, 0);
    CU_ASSERT_EQUAL(result, 1);
    
    /* check name change */
    result = iso_node_set_name(node1, "Another name");
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_STRING_EQUAL(node1->name, "Another name");
    
    /* addition of a 2nd node */
    node2 = calloc(1, sizeof(IsoNode));
    node2->name = strdup("A node to be added first");
    result = iso_dir_add_node(dir, node2, 0);
    CU_ASSERT_EQUAL(result, 2);
    
    result = iso_node_set_name(node2, "New name");
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_STRING_EQUAL(node2->name, "New name");
    
    /* and now try to give an existing name */
    result = iso_node_set_name(node2, "Another name");
    CU_ASSERT_EQUAL(result, ISO_NODE_NAME_NOT_UNIQUE);
    CU_ASSERT_STRING_EQUAL(node2->name, "New name");
    
    free(node1->name);
    free(node2->name);
    free(node1);
    free(node2);
    free(dir);
}

void add_node_suite()
{
	CU_pSuite pSuite = CU_add_suite("Node Test Suite", NULL, NULL);
	
	CU_add_test(pSuite, "iso_node_new_root()", test_iso_node_new_root);
    CU_add_test(pSuite, "iso_node_new_dir()", test_iso_node_new_dir);
    CU_add_test(pSuite, "iso_node_new_symlink()", test_iso_node_new_symlink);
	CU_add_test(pSuite, "iso_node_set_permissions()", test_iso_node_set_permissions);
    CU_add_test(pSuite, "iso_node_get_permissions()", test_iso_node_get_permissions);
    CU_add_test(pSuite, "iso_node_get_mode()", test_iso_node_get_mode);
    CU_add_test(pSuite, "iso_node_set_uid()", test_iso_node_set_uid);
    CU_add_test(pSuite, "iso_node_get_uid()", test_iso_node_get_uid);
    CU_add_test(pSuite, "iso_node_set_gid()", test_iso_node_set_gid);
    CU_add_test(pSuite, "iso_node_get_gid()", test_iso_node_get_gid);
    CU_add_test(pSuite, "iso_dir_add_node()", test_iso_dir_add_node);
    CU_add_test(pSuite, "iso_dir_get_node()", test_iso_dir_get_node);
    CU_add_test(pSuite, "iso_dir_get_children()", test_iso_dir_get_children);
    CU_add_test(pSuite, "iso_dir_iter_take()", test_iso_dir_iter_take);
    CU_add_test(pSuite, "iso_node_take()", test_iso_node_take);
    CU_add_test(pSuite, "iso_node_set_name()", test_iso_node_set_name);
}
