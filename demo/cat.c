/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "libisofs.h"
#include "fsource.h"

#include <stdio.h>
#include <stdlib.h>

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

    if (argc != 2) {
        fprintf(stderr, "Usage: cat /path/to/file\n");
        return 1;
    }

    /* create filesystem object */
    res = iso_local_filesystem_new(&fs);
    if (res < 0) {
        fprintf(stderr, "Can't get local fs object, err = %d\n", res);
        return 1;
    }

    res = fs->get_by_path(fs, argv[1], &file);
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
    return 0;
}
