/*
 * Mocked objects to simulate an input filesystem.
 */

#ifndef MOCKED_FSRC_H_
#define MOCKED_FSRC_H_

struct mock_file {
    IsoFilesystem *fs;
    struct mock_file *parent;
    struct stat atts;
    char *name;
    
    /* for links, link dest. For dirs, children */
    void *content;
};

/**
 * A mocked fs.
 */
int test_mocked_filesystem_new(IsoFilesystem **fs);

struct mock_file *test_mocked_fs_get_root(IsoFilesystem *fs);

int test_mocked_fs_add_dir(const char *name, struct mock_file *parent, 
                           struct stat atts, struct mock_file **dir);

int test_mocked_fs_add_symlink(const char *name, struct mock_file *p, 
          struct stat atts, const char *dest, struct mock_file **node);

#endif /*MOCKED_FSRC_H_*/
