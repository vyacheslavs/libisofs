/*
 * Little program to output the contents of an iso image.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "libisofs.h"

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
    IsoReadOpts *ropts;

    if (argc != 2) {
        printf ("You need to specify a valid path\n");
        return 1;
    }

    iso_init();
    iso_set_msgs_severities("NEVER", "ALL", "");
    
    result = iso_data_source_new_from_file(argv[1], &src);
    if (result < 0) {
        printf ("Error creating data source\n");
        return 1;
    }

    result = iso_read_opts_new(&ropts, 0);
    if (result < 0) {
        fprintf(stderr, "Error creating read options\n");
        return 1;
    }
    result = iso_image_filesystem_new(src, ropts, 1, &fs);
    iso_read_opts_free(ropts);
    if (result < 0) {
        printf ("Error creating filesystem\n");
        return 1;
    }
    
    printf("\nVOLUME INFORMATION\n");
    printf("==================\n\n");
    
    printf("Vol. id: %s\n", iso_image_fs_get_volume_id(fs));
    printf("Publisher: %s\n", iso_image_fs_get_publisher_id(fs));
    printf("Data preparer: %s\n", iso_image_fs_get_data_preparer_id(fs));
    printf("System: %s\n", iso_image_fs_get_system_id(fs));
    printf("Application: %s\n", iso_image_fs_get_application_id(fs));
    printf("Copyright: %s\n", iso_image_fs_get_copyright_file_id(fs));
    printf("Abstract: %s\n", iso_image_fs_get_abstract_file_id(fs));
    printf("Biblio: %s\n", iso_image_fs_get_biblio_file_id(fs));

    printf("\nDIRECTORY TREE\n");
    printf("==============\n");
    
    result = fs->get_root(fs, &root);
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
    iso_finish();
    return 0;
}
