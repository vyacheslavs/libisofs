/*
 * Little program to show how to create a multisession iso image.
 */

#include "libisofs.h"
#include "libburn/libburn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

void usage(char **argv)
{
    printf("%s LSS NWA DISC DIRECTORY OUTPUT\n", argv[0]);
}

int main(int argc, char **argv)
{
    int result;
    IsoImage *image;
    IsoDataSource *src;
    struct burn_source *burn_src;
    unsigned char buf[2048];
    FILE *fd;
    Ecma119WriteOpts opts = {
        1, /* level */ 
        1, /* rockridge */
        0, /* joliet */
        0, /* omit_version_numbers */
        0, /* allow_deep_paths */
        0, /* max_37_char_filenames */
        0, /* no_force_dots */
        0, /* allow_lowercase */
        0, /* allow_full_ascii */
        0, /* joliet_longer_paths */
        1, /* sort files */
        0, /* replace_dir_mode */
        0, /* replace_file_mode */
        0, /* replace_uid */
        0, /* replace_gid */
        0, /* dir_mode */
        0, /* file_mode */
        0, /* uid */
        0, /* gid */
        NULL, /* output charset */
        0, /* appendable */
        0, /* ms_block */
        NULL /* overwrite */
    };
    struct iso_read_opts ropts = {
        0, /* block */
        0, /* norock */
        0, /* nojoliet */
        0, /* preferjoliet */
        0, /* uid; */
        0, /* gid; */
        0, /* mode */
        "UTF-8" /* input_charset */
    };
	
    if (argc < 6) {
        usage(argv);
        return 1;
    }

    fd = fopen(argv[5], "w");
    if (!fd) {
        err(1, "error opening output file");
    }
    
    /* create the data source to accesss previous image */
    result = iso_data_source_new_from_file(argv[3], &src);
    if (result < 0) {
        printf ("Error creating data source\n");
        return 1;
    }
    
    /* create the image context */
    result = iso_image_new("volume_id", &image);
    if (result < 0) {
        printf ("Error creating image\n");
        return 1;
    }
    iso_image_set_msgs_severities(image, "NEVER", "ALL", "");
    iso_tree_set_follow_symlinks(image, 0);
    iso_tree_set_ignore_hidden(image, 0);
    iso_tree_set_stop_on_error(image, 0);
    
    /* import previous image */
    ropts.block = atoi(argv[1]);
    result = iso_image_import(image, src, &ropts, NULL);
    iso_data_source_unref(src);
    if (result < 0) {
        printf ("Error importing previous session %d\n", result);
        return 1;
    }
    
    /* add new dir */
    result = iso_tree_add_dir_rec(image, iso_image_get_root(image), argv[4]);
    if (result < 0) {
        printf ("Error adding directory %d\n", result);
        return 1;
    }
    
    /* generate a multisession image with new contents */
    opts.ms_block = atoi(argv[2]);
    opts.appendable = 1;
    result = iso_image_create_burn_source(image, &opts, &burn_src);
    if (result < 0) {
        printf ("Cant create image, error %d\n", result);
        return 1;
    }
    
    while (burn_src->read(burn_src, buf, 2048) == 2048) {
        fwrite(buf, 1, 2048, fd);
    }
    fclose(fd);
    burn_src->free_data(burn_src);
    free(burn_src);
    
    iso_image_unref(image);
	return 0;
}
