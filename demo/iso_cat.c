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
#include "fsource.h"
#include "fs_image.h"
#include "messages.h"

/*
 * Little test program to test filesystem implementations.
 * Outputs file contents to stdout!
 */

int main(int argc, char **argv)
{
    int res;
    IsoFilesystem *fs;
    IsoFileSource *file;
    struct stat info;
    IsoDataSource *src;
    struct libiso_msgs *messenger;
    struct iso_read_opts opts = {
        0, /* block */
        0, /* norock */
        0, /* nojoliet */
        0, /* preferjoliet */
        0, /* uid; */
        0, /* gid; */
        0, /* mode */
        "UTF-8" /* input_charset */
    };

    if (argc != 3) {
        fprintf(stderr, "Usage: isocat /path/to/image /path/to/file\n");
        return 1;
    }

    res = libiso_msgs_new(&messenger, 0);
    if (res <= 0) {
        printf ("Can't create messenger\n");
        return 1;
    }
    libiso_msgs_set_severities(messenger, LIBISO_MSGS_SEV_NEVER, 
                               LIBISO_MSGS_SEV_ALL, "", 0);
    
    res = iso_data_source_new_from_file(argv[1], &src);
    if (res < 0) {
        printf ("Error creating data source\n");
        return 1;
    }

    res = iso_image_filesystem_new(src, &opts, messenger, &fs);
    if (res < 0) {
        printf ("Error creating filesystem\n");
        return 1;
    }

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
    libiso_msgs_destroy(&messenger, 0);
    return 0;
}
