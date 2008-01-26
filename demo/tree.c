/*
 * Little program that import a directory and prints the resulting iso tree.
 */

#include "libisofs.h"
#include <stdio.h>
#include <stdlib.h>
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
print_dir(IsoDir *dir, int level) 
{
	int i;
	IsoDirIter *iter;
	IsoNode *node;
	char *sp = alloca(level * 2 + 1);
	
	for (i = 0; i < level * 2; i += 2) {
		sp[i] = '|';
		sp[i+1] = ' ';
	}
	
	sp[level * 2-1] = '-';
	sp[level * 2] = '\0';
	
	iso_dir_get_children(dir, &iter);
	while (iso_dir_iter_next(iter, &node) == 1) {
		
		if (iso_node_get_type(node) == LIBISO_DIR) {
			printf("%s+[D] ", sp);
			print_permissions(iso_node_get_permissions(node));
			printf(" %s\n", iso_node_get_name(node));
			print_dir((IsoDir*)node, level+1);
		} else if (iso_node_get_type(node) == LIBISO_FILE) {
			printf("%s-[F] ", sp);
			print_permissions(iso_node_get_permissions(node));
			printf(" %s\n", iso_node_get_name(node) );
		} else if (iso_node_get_type(node) == LIBISO_SYMLINK) {
			printf("%s-[L] ", sp);
			print_permissions(iso_node_get_permissions(node));
			printf(" %s -> %s \n", iso_node_get_name(node),
			       iso_symlink_get_dest((IsoSymlink*)node) );
		} else {
			printf("%s-[C] ", sp);
			print_permissions(iso_node_get_permissions(node));
			printf(" %s\n", iso_node_get_name(node) );
		} 
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
	
    printf("================= IMAGE =================\n");
	print_dir(iso_image_get_root(image), 0);
	printf("\n\n");
	
    iso_image_unref(image);
    iso_finish();
    return 0;
}
