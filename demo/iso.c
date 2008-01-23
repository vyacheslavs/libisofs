/*
 * Little program to show how to create an iso image from a local
 * directory.
 */

#include "libisofs.h"
#include "libburn/libburn.h"

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

const char * const optstring = "JRIL:b:hV:";
extern char *optarg;
extern int optind;

void usage(char **argv)
{
    printf("%s [OPTIONS] DIRECTORY OUTPUT\n", argv[0]);
}

void help()
{
    printf(
        "Options:\n"
        "  -J        Add Joliet support\n"
        "  -R        Add Rock Ridge support\n"
        "  -I        Add ISO 9660:1999 support\n"
        "  -V label  Volume Label\n"
        "  -L <num>  Set the ISO level (1 or 2)\n"
        "  -b file   Specifies a boot image to add to image\n"
        "  -h        Print this message\n"
    );
}

int callback(IsoFileSource *src)
{
    char *path = iso_file_source_get_path(src);
    printf("CALLBACK: %s\n", path);
    free(path);
    return 1;
}

int main(int argc, char **argv)
{
    int result;
    int c;
    IsoImage *image;
    struct burn_source *burn_src;
    unsigned char buf[2048];
    FILE *fd;
    char *volid = "VOLID";
    char *boot_img = NULL;
    
    Ecma119WriteOpts opts = {
        1, /* level */ 
        0, /* rockridge */
        0, /* joliet */
        0, /* iso1999 */
        0, /* omit_version_numbers */
        0, /* allow_deep_paths */
        0, /* allow_longer_paths */
        0, /* max_37_char_filenames */
        0, /* no_force_dots */
        0, /* allow_lowercase */
        0, /* allow_full_ascii */
        0, /* joliet_longer_paths */
        0, /* sort files */
        0, /* replace_dir_mode */
        0, /* replace_file_mode */
        0, /* replace_uid */
        0, /* replace_gid */
        0, /* dir_mode */
        0, /* file_mode */
        0, /* uid */
        0, /* gid */
        0, /* replace_timestamps */
        0, /* timestamp */
        NULL, /* output charset */
        0, /* appendable */
        0, /* ms_block */
        NULL, /* overwrite */
        1024 /* fifo_size */
    };

    while ((c = getopt(argc, argv, optstring)) != -1) {
        switch(c) {
        case 'h':
            usage(argv);
            help();
            exit(0);
            break;
        case 'J':
            opts.joliet = 1;
            break;
        case 'R':
            opts.rockridge = 1;
            break;
        case 'I':
            opts.iso1999 = 1;
            break;
        case 'L':
            opts.level = atoi(optarg);
            break;
        case 'b':
            boot_img = optarg;
            break;
        case 'V':
            volid = optarg;
            break;
        case '?':
            usage(argv);
            exit(1);
            break;
        }
    }
	
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

    result = iso_init();
    if (result < 0) {
        printf ("Can't initialize libisofs\n");
        return 1;
    }
    iso_set_msgs_severities("NEVER", "ALL", "");
    
    result = iso_image_new(volid, &image);
    if (result < 0) {
        printf ("Error creating image\n");
        return 1;
    }
    iso_tree_set_follow_symlinks(image, 0);
    iso_tree_set_ignore_hidden(image, 0);
    iso_tree_set_ignore_special(image, 0);
    iso_set_abort_severity("SORRY");
    iso_tree_set_report_callback(image, callback);
    
    result = iso_tree_add_dir_rec(image, iso_image_get_root(image), argv[optind]);
    if (result < 0) {
        printf ("Error adding directory %d\n", result);
        return 1;
    }
    
    if (boot_img) {
        /* adds El-Torito boot info. Tunned for isolinux */
        ElToritoBootImage *bootimg;
        result = iso_image_set_boot_image(image, boot_img, ELTORITO_NO_EMUL,
                                     "/isolinux/boot.cat", &bootimg);
        if (result < 0) {
            printf ("Error adding boot image %d\n", result);
            return 1;
        }
        el_torito_set_load_size(bootimg, 4);
        el_torito_patch_isolinux_image(bootimg);
    }
    
    result = iso_image_create_burn_source(image, &opts, &burn_src);
    if (result < 0) {
        printf ("Cant create image, error %d\n", result);
        return 1;
    }
    
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
