/*
 * Little program to output the contents of an iso image.
 * Note that this is not an API example, but a little program for test
 * purposes.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "messages.h"
#include "libisofs.h"
#include "fs_image.h"

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
    iso_file_source_lstat(file, &info);
    print_type(info.st_mode);
    print_permissions(info.st_mode);
    //printf(" {%ld,%ld} ", (long)info.st_dev, (long)info.st_ino);
    name = iso_file_source_get_name(file);
    printf(" %s", name);
    free(name);
    if (S_ISLNK(info.st_mode)) {
        char buf[PATH_MAX];
        iso_file_source_readlink(file, buf, PATH_MAX);
        printf(" -> %s\n", buf);
    }
    printf("\n");
}

static void
print_dir(IsoFileSource *dir, int level)
{
    int ret, i;
    IsoFileSource *file;
    struct stat info;
    char *sp = alloca(level * 2 + 1);

    for (i = 0; i < level * 2; i += 2) {
        sp[i] = '|';
        sp[i+1] = ' ';
    }

    sp[level * 2-1] = '-';
    sp[level * 2] = '\0';

    ret = iso_file_source_open(dir);
    if (ret < 0) {
        printf ("Can't open dir %d\n", ret);
    }
    while ((ret = iso_file_source_readdir(dir, &file)) == 1) {
        printf("%s", sp);
        print_file_src(file);
        ret = iso_file_source_lstat(file, &info);
        if (ret < 0) {
            break;
        }
        if (S_ISDIR(info.st_mode)) {
            print_dir(file, level + 1);
        }
        iso_file_source_unref(file);
    }
    iso_file_source_close(dir);
    if (ret < 0) {
        printf ("Can't print dir\n");
    }
}

int main(int argc, char **argv)
{
    int result;
    IsoImageFilesystem *fs;
    IsoDataSource *src;
    IsoFileSource *root;
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

    if (argc != 2) {
        printf ("You need to specify a valid path\n");
        return 1;
    }

    result = libiso_msgs_new(&messenger, 0);
    if (result <= 0) {
        printf ("Can't create messenger\n");
        return 1;
    }
    libiso_msgs_set_severities(messenger, LIBISO_MSGS_SEV_NEVER, 
                               LIBISO_MSGS_SEV_ALL, "", 0);
    
    result = iso_data_source_new_from_file(argv[1], &src);
    if (result < 0) {
        printf ("Error creating data source\n");
        return 1;
    }

    result = iso_image_filesystem_new(src, &opts, messenger, &fs);
    if (result < 0) {
        printf ("Error creating filesystem\n");
        return 1;
    }

    result = fs->fs.get_root((IsoFilesystem*)fs, &root);
    if (result < 0) {
        printf ("Can't get root %d\n", result);
        return 1;
    }
    //print_file_src(root);
    print_dir(root, 0);
    iso_file_source_unref(root);
    
    fs->close(fs);
    iso_filesystem_unref((IsoFilesystem*)fs);
    iso_data_source_unref(src);
    libiso_msgs_destroy(&messenger, 0);
    return 0;
}
