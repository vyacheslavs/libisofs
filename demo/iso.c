/*
 * Little program to show how to create an iso image from a local
 * directory.
 */

#include "libisofs.h"
#include "libburn/libburn.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

void usage(char **argv)
{
    printf("%s [OPTIONS] DIRECTORY OUTPUT\n", argv[0]);
}

int main(int argc, char **argv)
{
    int result;
    IsoImage *image;
    struct burn_source *burn_src;
    unsigned char buf[2048];
    FILE *fd;
    Ecma119WriteOpts opts = {
        1, /* level */ 
        1, /* rockridge */
        0, /* omit_version_numbers */
        0, /* allow_deep_paths */
        0, /* sort files */
        0, /* replace_dir_mode */
        0, /* replace_file_mode */
        0, /* replace_uid */
        0, /* replace_gid */
        0, /* dir_mode */
        0, /* file_mode */
        0, /* uid */
        0  /* gid */
    };
	
    if (argc < 2) {
        printf ("Please pass directory from which to build ISO\n");
        usage(argv);
        return 1;
    }
    if (argc < 3) {
        printf ("Please supply output file\n");
        usage(argv);
        return 1;
    }
    
    fd = fopen(argv[optind+1], "w");
    if (!fd) {
        err(1, "error opening output file");
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
    
    result = iso_image_create(image, &opts, &burn_src);
    if (result < 0) {
        printf ("Cant create image, error %d\n", result);
        return 1;
    }
    
    while (burn_src->read(burn_src, buf, 2048) == 2048) {
        fwrite(buf, 1, 2048, fd);
    }
    fclose(fd);
    burn_src->free_data(burn_src);
    
    iso_image_unref(image);
	return 0;
}
