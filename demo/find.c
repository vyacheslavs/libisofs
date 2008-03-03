/*
 * Little program that import a directory, find matching nodes and prints the 
 * resulting iso tree.
 */

#include "libisofs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static void 
print_dir(IsoDir *dir) 
{
	IsoDirIter *iter;
	IsoNode *node;
	IsoFindCondition *cond;
	
	cond = iso_new_find_conditions_name("*a*");
	iso_dir_find_children(dir, cond, &iter);
	while (iso_dir_iter_next(iter, &node) == 1) {
		printf(" %s\n", iso_node_get_name(node));
	}
	iso_dir_iter_free(iter);
}

int main(int argc, char **argv)
{
	int result;
    IsoImage *image;
	
	if (argc != 2) {
		printf ("You need to specify a valid path\n");
		return 1;
	}

    iso_init();
    iso_set_msgs_severities("NEVER", "ALL", "");
    
    result = iso_image_new("volume_id", &image);
    if (result < 0) {
        printf ("Error creating image\n");
        return 1;
    }
	
    result = iso_tree_add_dir_rec(image, iso_image_get_root(image), argv[1]);
    if (result < 0) {
        printf ("Error adding directory %d\n", result);
        return 1;
    }
	
	print_dir(iso_image_get_root(image));
	
    iso_image_unref(image);
    iso_finish();
    return 0;
}
