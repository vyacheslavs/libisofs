/*
 * Mocked objects to simulate an input filesystem.
 */

#ifndef MOCKED_FSRC_H_
#define MOCKED_FSRC_H_

/**
 * A mocked fs with a registry of files, that you can add via
 * test_mocked_fs_add_fsrc().
 */
int test_mocked_filesystem_new(IsoFilesystem **fs);

int test_mocked_fs_add_file(IsoFilesystem *fs, const char *path, 
                            struct stat info);

int test_mocked_fs_add_dir(IsoFilesystem *fs, const char *path, 
                            struct stat info);

int test_mocked_fs_add_symlink(IsoFilesystem *fs, const char *path, 
                            struct stat info, const char *dest);

#endif /*MOCKED_FSRC_H_*/
