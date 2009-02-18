/*
 * Little program to show how to create a multisession iso image.
 */

#define LIBISOFS_WITHOUT_LIBBURN yes
#include "libisofs.h"

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
    IsoWriteOpts *opts;
    IsoReadOpts *ropts;
    uint32_t ms_block;
	
    if (argc < 6) {
        usage(argv);
        return 1;
    }

    fd = fopen(argv[5], "w");
    if (!fd) {
        err(1, "error opening output file");
    }

    iso_init();
    iso_set_msgs_severities("NEVER", "ALL", "");
    
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
    iso_tree_set_follow_symlinks(image, 0);
    iso_tree_set_ignore_hidden(image, 0);
    
    /* import previous image */
    result = iso_read_opts_new(&ropts, 0);
    if (result < 0) {
        fprintf(stderr, "Error creating read options\n");
        return 1;
    }
    iso_read_opts_set_start_block(ropts, atoi(argv[1]));
    result = iso_image_import(image, src, ropts, NULL);
    iso_read_opts_free(ropts);
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
    result = iso_write_opts_new(&opts, 1);
    if (result < 0) {
        printf("Cant create write opts, error %d\n", result);
        return 1;
    }
    
    /* round up to 32kb aligment = 16 block */
    ms_block =  atoi(argv[2]);
    iso_write_opts_set_ms_block(opts, ms_block);
    iso_write_opts_set_appendable(opts, 1);

    result = iso_image_create_burn_source(image, opts, &burn_src);
    if (result < 0) {
        printf ("Cant create image, error %d\n", result);
        return 1;
    }
    iso_write_opts_free(opts);
    
    while (burn_src->read_xt(burn_src, buf, 2048) == 2048) {
        fwrite(buf, 1, 2048, fd);
    }
    fclose(fd);
    burn_src->free_data(burn_src);
    free(burn_src);
    
    iso_image_unref(image);
    iso_finish();
	return 0;
}
