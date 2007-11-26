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
 * 
 */

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
    printf(" %s ",perm);
}

static void
print_type(mode_t mode)
{
    char type[1];

    switch(mode & S_IFMT) {
    case S_IFSOCK: printf("[S] "); break;
    case S_IFLNK: printf("[L] "); break;
    case S_IFREG: printf("[R] "); break;
    case S_IFBLK: printf("[B] "); break;
    case S_IFDIR: printf("[D] "); break;
    case S_IFIFO: printf("[F] "); break; 
    }
}

static void
print_file_src(IsoFileSource *file)
{
    struct stat info;
    char *name;
    file->lstat(file, &info);
    print_type(info.st_mode);
    print_permissions(info.st_mode);
    name = file->get_name(file);
    printf(" %s\n", name);
    free(name);
}

int main(int argc, char **argv)
{
    int res;
    IsoFilesystem *fs;
    IsoFileSource *dir;
    IsoFileSource *file;
    struct stat info;

    if (argc != 2) {
        fprintf(stderr, "Usage: iso /path/to/dir\n");
        return 1;
    }

    /* create filesystem object */
    res = iso_local_filesystem_new(&fs);
    if (res < 0) {
        fprintf(stderr, "Can't get local fs object, err = %d\n", res);
        return 1;
    }

    res = fs->get_by_path(fs, argv[1], &dir);
    if (res < 0) {
        fprintf(stderr, "Can't get file, err = %d\n", res);
        return 1;
    }

    res = dir->lstat(dir, &info);
    if (res < 0) {
        fprintf(stderr, "Can't stat file, err = %d\n", res);
        return 1;
    }

    //TODO check dir vs file
    if (S_ISDIR(info.st_mode)) {
        res = dir->open(dir);
        if (res < 0) {
            fprintf(stderr, "Can't open file, err = %d\n", res);
            return 1;
        }

        while (dir->readdir(dir, &file) == 1) {
            print_file_src(file);
        }

        res = dir->close(dir);
        if (res < 0) {
            fprintf(stderr, "Can't close file, err = %d\n", res);
            return 1;
        }
    } else {
        print_file_src(dir);
    }


    return 0;
}
