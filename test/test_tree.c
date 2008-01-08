/*
 * Unit test for node.h
 */

#include "libisofs.h"
#include "node.h"
#include "error.h"
#include "image.h"

#include "test.h"
#include "mocked_fsrc.h"

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

static
void test_iso_tree_add_node_dir()
{
    int result;
    IsoDir *root;
    IsoNode *node1, *node2, *node3, *node4;
    IsoImage *image;
    IsoFilesystem *fs;
    struct stat info;
    struct mock_file *mroot, *dir1, *dir2;
    
    result = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(result, 1);
    root = iso_image_get_root(image);
    CU_ASSERT_PTR_NOT_NULL(root);
    
    /* replace image filesystem with out mockep one */
    iso_filesystem_unref(image->fs);
    result = test_mocked_filesystem_new(&fs);
    CU_ASSERT_EQUAL(result, 1);
    image->fs = fs;
    mroot = test_mocked_fs_get_root(fs);
    
    /* add some files to the filesystem */
    info.st_mode = S_IFDIR | 0550;
    info.st_uid = 20;
    info.st_gid = 21;
    info.st_atime = 234523;
    info.st_ctime = 23432432;
    info.st_mtime = 1111123;
    result = test_mocked_fs_add_dir("dir", mroot, info, &dir1);
    CU_ASSERT_EQUAL(result, 1);
    
    info.st_mode = S_IFDIR | 0555;
    info.st_uid = 30;
    info.st_gid = 31;
    info.st_atime = 3234523;
    info.st_ctime = 3234432;
    info.st_mtime = 3111123;
    result = test_mocked_fs_add_dir("a child node", dir1, info, &dir2);
    CU_ASSERT_EQUAL(result, 1);
    
    info.st_mode = S_IFDIR | 0750;
    info.st_uid = 40;
    info.st_gid = 41;
    info.st_atime = 4234523;
    info.st_ctime = 4234432;
    info.st_mtime = 4111123;
    result = test_mocked_fs_add_dir("another one", dir1, info, &dir2);
    CU_ASSERT_EQUAL(result, 1);
    
    info.st_mode = S_IFDIR | 0755;
    info.st_uid = 50;
    info.st_gid = 51;
    info.st_atime = 5234523;
    info.st_ctime = 5234432;
    info.st_mtime = 5111123;
    result = test_mocked_fs_add_dir("zzzz", mroot, info, &dir2);
    CU_ASSERT_EQUAL(result, 1);
    
    /* and now insert those files to the image */
    result = iso_tree_add_node(image, root, "/dir", &node1);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(root->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(root->children, node1);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_EQUAL(node1->parent, root);
    CU_ASSERT_EQUAL(node1->type, LIBISO_DIR);
    CU_ASSERT_STRING_EQUAL(node1->name, "dir");
    CU_ASSERT_EQUAL(node1->mode, S_IFDIR | 0550);
    CU_ASSERT_EQUAL(node1->uid, 20);
    CU_ASSERT_EQUAL(node1->gid, 21);
    CU_ASSERT_EQUAL(node1->atime, 234523);
    CU_ASSERT_EQUAL(node1->ctime, 23432432);
    CU_ASSERT_EQUAL(node1->mtime, 1111123);
    CU_ASSERT_PTR_NULL(((IsoDir*)node1)->children);
    CU_ASSERT_EQUAL(((IsoDir*)node1)->nchildren, 0);
    
    result = iso_tree_add_node(image, root, "/dir/a child node", &node2);
    CU_ASSERT_EQUAL(result, 2);
    CU_ASSERT_EQUAL(root->nchildren, 2);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node1);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_EQUAL(node1->parent, root);
    CU_ASSERT_PTR_EQUAL(node2->parent, root);
    CU_ASSERT_EQUAL(node2->type, LIBISO_DIR);
    CU_ASSERT_STRING_EQUAL(node2->name, "a child node");
    CU_ASSERT_EQUAL(node2->mode, S_IFDIR | 0555);
    CU_ASSERT_EQUAL(node2->uid, 30);
    CU_ASSERT_EQUAL(node2->gid, 31);
    CU_ASSERT_EQUAL(node2->atime, 3234523);
    CU_ASSERT_EQUAL(node2->ctime, 3234432);
    CU_ASSERT_EQUAL(node2->mtime, 3111123);
    CU_ASSERT_PTR_NULL(((IsoDir*)node2)->children);
    CU_ASSERT_EQUAL(((IsoDir*)node2)->nchildren, 0);
    
    result = iso_tree_add_node(image, root, "/dir/another one", &node3);
    CU_ASSERT_EQUAL(result, 3);
    CU_ASSERT_EQUAL(root->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node3);
    CU_ASSERT_PTR_EQUAL(node3->next, node1);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_EQUAL(node1->parent, root);
    CU_ASSERT_PTR_EQUAL(node2->parent, root);
    CU_ASSERT_PTR_EQUAL(node3->parent, root);
    CU_ASSERT_EQUAL(node3->type, LIBISO_DIR);
    CU_ASSERT_STRING_EQUAL(node3->name, "another one");
    CU_ASSERT_EQUAL(node3->mode, S_IFDIR | 0750);
    CU_ASSERT_EQUAL(node3->uid, 40);
    CU_ASSERT_EQUAL(node3->gid, 41);
    CU_ASSERT_EQUAL(node3->atime, 4234523);
    CU_ASSERT_EQUAL(node3->ctime, 4234432);
    CU_ASSERT_EQUAL(node3->mtime, 4111123);
    CU_ASSERT_PTR_NULL(((IsoDir*)node3)->children);
    CU_ASSERT_EQUAL(((IsoDir*)node3)->nchildren, 0);
    
    result = iso_tree_add_node(image, root, "/zzzz", &node4);
    CU_ASSERT_EQUAL(result, 4);
    CU_ASSERT_EQUAL(root->nchildren, 4);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node3);
    CU_ASSERT_PTR_EQUAL(node3->next, node1);
    CU_ASSERT_PTR_EQUAL(node1->next, node4);
    CU_ASSERT_PTR_NULL(node4->next);
    CU_ASSERT_PTR_EQUAL(node1->parent, root);
    CU_ASSERT_PTR_EQUAL(node2->parent, root);
    CU_ASSERT_PTR_EQUAL(node3->parent, root);
    CU_ASSERT_PTR_EQUAL(node4->parent, root);
    CU_ASSERT_EQUAL(node4->type, LIBISO_DIR);
    CU_ASSERT_STRING_EQUAL(node4->name, "zzzz");
    CU_ASSERT_EQUAL(node4->mode, S_IFDIR | 0755);
    CU_ASSERT_EQUAL(node4->uid, 50);
    CU_ASSERT_EQUAL(node4->gid, 51);
    CU_ASSERT_EQUAL(node4->atime, 5234523);
    CU_ASSERT_EQUAL(node4->ctime, 5234432);
    CU_ASSERT_EQUAL(node4->mtime, 5111123);
    CU_ASSERT_PTR_NULL(((IsoDir*)node4)->children);
    CU_ASSERT_EQUAL(((IsoDir*)node4)->nchildren, 0);
    
    iso_image_unref(image);
}

static
void test_iso_tree_add_node_link()
{
    int result;
    IsoDir *root;
    IsoNode *node1, *node2, *node3;
    IsoImage *image;
    IsoFilesystem *fs;
    struct stat info;
    struct mock_file *mroot, *link;
    
    result = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(result, 1);
    root = iso_image_get_root(image);
    CU_ASSERT_PTR_NOT_NULL(root);
    
    /* replace image filesystem with out mockep one */
    iso_filesystem_unref(image->fs);
    result = test_mocked_filesystem_new(&fs);
    CU_ASSERT_EQUAL(result, 1);
    image->fs = fs;
    mroot = test_mocked_fs_get_root(fs);
    
    /* add some files to the filesystem */
    info.st_mode = S_IFLNK | 0777;
    info.st_uid = 12;
    info.st_gid = 13;
    info.st_atime = 123444;
    info.st_ctime = 123555;
    info.st_mtime = 123666;
    result = test_mocked_fs_add_symlink("link1", mroot, info, "/home/me", &link);
    CU_ASSERT_EQUAL(result, 1);
    
    info.st_mode = S_IFLNK | 0555;
    info.st_uid = 22;
    info.st_gid = 23;
    info.st_atime = 223444;
    info.st_ctime = 223555;
    info.st_mtime = 223666;
    result = test_mocked_fs_add_symlink("another link", mroot, info, "/", &link);
    CU_ASSERT_EQUAL(result, 1);
    
    info.st_mode = S_IFLNK | 0750;
    info.st_uid = 32;
    info.st_gid = 33;
    info.st_atime = 323444;
    info.st_ctime = 323555;
    info.st_mtime = 323666;
    result = test_mocked_fs_add_symlink("this will be the last", mroot, info, "/etc", &link);
    CU_ASSERT_EQUAL(result, 1);
    
    /* and now insert those files to the image */
    result = iso_tree_add_node(image, root, "/link1", &node1);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_EQUAL(root->nchildren, 1);
    CU_ASSERT_PTR_EQUAL(root->children, node1);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_EQUAL(node1->parent, root);
    CU_ASSERT_EQUAL(node1->type, LIBISO_SYMLINK);
    CU_ASSERT_STRING_EQUAL(node1->name, "link1");
    CU_ASSERT_EQUAL(node1->mode, S_IFLNK | 0777);
    CU_ASSERT_EQUAL(node1->uid, 12);
    CU_ASSERT_EQUAL(node1->gid, 13);
    CU_ASSERT_EQUAL(node1->atime, 123444);
    CU_ASSERT_EQUAL(node1->ctime, 123555);
    CU_ASSERT_EQUAL(node1->mtime, 123666);
    CU_ASSERT_STRING_EQUAL(((IsoSymlink*)node1)->dest, "/home/me");
    
    result = iso_tree_add_node(image, root, "/another link", &node2);
    CU_ASSERT_EQUAL(result, 2);
    CU_ASSERT_EQUAL(root->nchildren, 2);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node1);
    CU_ASSERT_PTR_NULL(node1->next);
    CU_ASSERT_PTR_EQUAL(node1->parent, root);
    CU_ASSERT_PTR_EQUAL(node2->parent, root);
    CU_ASSERT_EQUAL(node2->type, LIBISO_SYMLINK);
    CU_ASSERT_STRING_EQUAL(node2->name, "another link");
    CU_ASSERT_EQUAL(node2->mode, S_IFLNK | 0555);
    CU_ASSERT_EQUAL(node2->uid, 22);
    CU_ASSERT_EQUAL(node2->gid, 23);
    CU_ASSERT_EQUAL(node2->atime, 223444);
    CU_ASSERT_EQUAL(node2->ctime, 223555);
    CU_ASSERT_EQUAL(node2->mtime, 223666);
    CU_ASSERT_STRING_EQUAL(((IsoSymlink*)node2)->dest, "/");
    
    result = iso_tree_add_node(image, root, "/this will be the last", &node3);
    CU_ASSERT_EQUAL(result, 3);
    CU_ASSERT_EQUAL(root->nchildren, 3);
    CU_ASSERT_PTR_EQUAL(root->children, node2);
    CU_ASSERT_PTR_EQUAL(node2->next, node1);
    CU_ASSERT_PTR_EQUAL(node1->next, node3);
    CU_ASSERT_PTR_NULL(node3->next);
    CU_ASSERT_PTR_EQUAL(node1->parent, root);
    CU_ASSERT_PTR_EQUAL(node2->parent, root);
    CU_ASSERT_PTR_EQUAL(node3->parent, root);
    CU_ASSERT_EQUAL(node3->type, LIBISO_SYMLINK);
    CU_ASSERT_STRING_EQUAL(node3->name, "this will be the last");
    CU_ASSERT_EQUAL(node3->mode, S_IFLNK | 0750);
    CU_ASSERT_EQUAL(node3->uid, 32);
    CU_ASSERT_EQUAL(node3->gid, 33);
    CU_ASSERT_EQUAL(node3->atime, 323444);
    CU_ASSERT_EQUAL(node3->ctime, 323555);
    CU_ASSERT_EQUAL(node3->mtime, 323666);
    CU_ASSERT_STRING_EQUAL(((IsoSymlink*)node3)->dest, "/etc");
    
    iso_image_unref(image);
}

static
void test_iso_tree_path_to_node()
{
    int result;
    IsoDir *root;
    IsoDir *node1, *node2, *node11;
    IsoNode *node;
    IsoImage *image;
    IsoFilesystem *fs;
    
    result = iso_image_new("volume_id", &image);
    CU_ASSERT_EQUAL(result, 1);
    root = iso_image_get_root(image);
    CU_ASSERT_PTR_NOT_NULL(root);
    
    /* replace image filesystem with out mockep one */
    iso_filesystem_unref(image->fs);
    result = test_mocked_filesystem_new(&fs);
    CU_ASSERT_EQUAL(result, 1);
    image->fs = fs;
    
    /* add some files */
    result = iso_tree_add_new_dir(root, "Dir1", &node1);
    CU_ASSERT_EQUAL(result, 1);
    result = iso_tree_add_new_dir(root, "Dir2", (IsoDir**)&node2);
    CU_ASSERT_EQUAL(result, 2);
    result = iso_tree_add_new_dir((IsoDir*)node1, "Dir11", (IsoDir**)&node11);
    CU_ASSERT_EQUAL(result, 1);
    
    /* retrive some items */
    result = iso_tree_path_to_node(image, "/", &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, root);
    result = iso_tree_path_to_node(image, "/Dir1", &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node1);
    result = iso_tree_path_to_node(image, "/Dir2", &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node2);
    result = iso_tree_path_to_node(image, "/Dir1/Dir11", &node);
    CU_ASSERT_EQUAL(result, 1);
    CU_ASSERT_PTR_EQUAL(node, node11);
    
    /* some failtures */
    result = iso_tree_path_to_node(image, "/Dir2/Dir11", &node);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_PTR_NULL(node);
    
    iso_image_unref(image);
}

void add_tree_suite()
{
	CU_pSuite pSuite = CU_add_suite("Iso Tree Suite", NULL, NULL);
   
    CU_add_test(pSuite, "iso_tree_add_new_dir()", test_iso_tree_add_new_dir);
    CU_add_test(pSuite, "iso_tree_add_new_symlink()", test_iso_tree_add_new_symlink);
    CU_add_test(pSuite, "iso_tree_add_new_special()", test_iso_tree_add_new_special);
    CU_add_test(pSuite, "iso_tree_add_node() [1. dir]", test_iso_tree_add_node_dir);
    CU_add_test(pSuite, "iso_tree_add_node() [2. symlink]", test_iso_tree_add_node_link);
    CU_add_test(pSuite, "iso_tree_path_to_node()", test_iso_tree_path_to_node);
    
}
