/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */


#include <stdio.h>
#include <stdlib.h>

#include "libisofs.h"

/*
 * Little test program that extracts a file form a given ISO image.
 * Outputs file contents to stdout!
 */

int main(int argc, char **argv)
{
    int res;
    IsoFilesystem *fs;
    IsoFileSource *file;
    struct stat info;
    IsoDataSource *src;
    IsoReadOpts *opts;

    if (argc != 3) {
        fprintf(stderr, "Usage: isocat /path/to/image /path/to/file\n");
        return 1;
    }

    res = iso_init();
    if (res < 0) {
        fprintf(stderr, "Can't init libisofs\n");
        return 1;
    }
    
    res = iso_data_source_new_from_file(argv[1], &src);
    if (res < 0) {
        fprintf(stderr, "Error creating data source\n");
        return 1;
    }

    res = iso_read_opts_new(&opts, 0);
    if (res < 0) {
        fprintf(stderr, "Error creating read options\n");
        return 1;
    }
    res = iso_image_filesystem_new(src, opts, 1, &fs);
    if (res < 0) {
        fprintf(stderr, "Error creating filesystem\n");
        return 1;
    }
    iso_read_opts_free(opts);

    res = fs->get_by_path(fs, argv[2], &file);
    if (res < 0) {
        fprintf(stderr, "Can't get file, err = %d\n", res);
        return 1;
    }

    res = iso_file_source_lstat(file, &info);
    if (res < 0) {
        fprintf(stderr, "Can't stat file, err = %d\n", res);
        return 1;
    }

    if (S_ISDIR(info.st_mode)) {
        fprintf(stderr, "Path refers to a directory!!\n");
        return 1;
    } else {
        char buf[1024];
        res = iso_file_source_open(file);
        if (res < 0) {
            fprintf(stderr, "Can't open file, err = %d\n", res);
            return 1;
        }
        while ((res = iso_file_source_read(file, buf, 1024)) > 0) {
            fwrite(buf, 1, res, stdout);
        }
        if (res < 0) {
            fprintf(stderr, "Error reading, err = %d\n", res);
            return 1;
        }
        iso_file_source_close(file);
    }
    
    iso_file_source_unref(file);
    iso_filesystem_unref(fs);
    iso_data_source_unref(src);
    iso_finish();
    return 0;
}
