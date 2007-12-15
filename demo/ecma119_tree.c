/*
 * Little program that imports a directory to iso image, generates the
 * ecma119 low level tree and prints it.
 * Note that this is not an API example, but a little program for test
 * purposes.
 */

#include "libisofs.h"
#include "ecma119.h"
#include "ecma119_tree.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static void
print_permissions(mode_t mode)
{
	char perm[10];
	
	//TODO suid, sticky...
	
	perm[9] = '\0';
	perm[8] = mode & S_IXOTH ? 'x' : '-';
	perm[7] = mode & S_IWOTH ? 'w' : '-';
	perm[6] = mode & S_IROTH ? 'r' : '-';
	perm[5] = mode & S_IXGRP ? 'x' : '-';
	perm[4] = mode & S_IWGRP ? 'w' : '-';
	perm[3] = mode & S_IRGRP ? 'r' : '-';
	perm[2] = mode & S_IXUSR ? 'x' : '-';
	perm[1] = mode & S_IWUSR ? 'w' : '-';
	perm[0] = mode & S_IRUSR ? 'r' : '-';
	printf("[%s]",perm);
}

static void 
print_dir(Ecma119Node *dir, int level) 
{
	int i;
	char sp[level * 2 + 1];
	
	for (i = 0; i < level * 2; i += 2) {
		sp[i] = '|';
		sp[i+1] = ' ';
	}
	
	sp[level * 2-1] = '-';
	sp[level * 2] = '\0';
	
    for (i = 0; i < dir->info.dir.nchildren; i++) {
		Ecma119Node *child = dir->info.dir.children[i];
        
		if (child->type == ECMA119_DIR) {
			printf("%s+[D] ", sp);
			print_permissions(iso_node_get_permissions(child->node));
			printf(" %s\n", child->iso_name);
			print_dir(child, level+1);
		} else if (child->type == ECMA119_FILE) {
			printf("%s-[F] ", sp);
            print_permissions(iso_node_get_permissions(child->node));
            printf(" %s {%p}\n", child->iso_name, (void*)child->info.file);
		} else {
			printf("%s-[????] ", sp);
		} 
	}
}

int main(int argc, char **argv)
{
	int result;
    IsoImage *image;
    Ecma119Image *ecma119;
    Ecma119Node *tree;
	
	if (argc != 2) {
		printf ("You need to specify a valid path\n");
		return 1;
	}
	
    result = iso_image_new("volume_id", &image);
    if (result < 0) {
        printf ("Error creating image\n");
        return 1;
    }
	iso_image_set_msgs_severities(image, "NEVER", "ALL", "");
	
    result = iso_tree_add_dir_rec(image, iso_image_get_root(image), argv[1]);
    if (result < 0) {
        printf ("Error adding directory %d\n", result);
        return 1;
    }
    
    ecma119 = calloc(1, sizeof(Ecma119Image));
    ecma119->iso_level = 1;
	
    /* create low level tree */
    result = ecma119_tree_create(ecma119, (IsoNode*)iso_image_get_root(image));
    if (result < 0) {
        printf ("Error creating ecma-119 tree: %d\n", result);
        return 1;
    }
    
    printf("================= ECMA-119 TREE =================\n");
	print_dir(ecma119->root, 0);
	printf("\n\n");
	
    ecma119_node_free(ecma119->root);
    iso_image_unref(image);
	return 0;
}
