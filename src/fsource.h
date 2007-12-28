/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_FSOURCE_H_
#define LIBISO_FSOURCE_H_

/*
 * Definitions for the file sources.
 */

/*
 * Some functions here will be moved to libisofs.h when we expose Sources.
 */

#include <sys/stat.h>

typedef struct Iso_File_Source IsoFileSource;
typedef struct Iso_Filesystem IsoFilesystem;

/**
 * See IsoFilesystem->get_id() for info about this.
 */
extern unsigned int iso_fs_global_id;

struct Iso_Filesystem
{

    /**
     * 
     * @return
     *    1 on success, < 0 on error
     */
    int (*get_root)(IsoFilesystem *fs, IsoFileSource **root);

    /**
     * 
     * @return
     *     1 success, < 0 error
     */
    int (*get_by_path)(IsoFilesystem *fs, const char *path,
                       IsoFileSource **file);

    /**
     * Get filesystem identifier. 
     * 
     * If the filesystem is able to generate correct values of the st_dev
     * and st_ino fields for the struct stat of each file, this should
     * return an unique number, greater than 0. 
     * 
     * To get a identifier for your filesystem implementation you should 
     * use iso_fs_global_id, incrementing it by one each time.
     * 
     * Otherwise, if you can't ensure values in the struct stat are valid,
     * this should return 0.
     */
    unsigned int (*get_id)(IsoFilesystem *fs);

    void (*free)(IsoFilesystem *fs);

    /* TODO each file will take a ref to IsoFilesystem, so maybe a 64bits
     * integer is a better choose for this */
    unsigned int refcount;
    void *data;
};

typedef struct IsoFileSource_Iface
{

    /**
     * Get the path, relative to the filesystem this file source
     * belongs to.
     * 
     * @return
     *     the path, that belong to the IsoFileSource and should not be
     *     freed by the user.
     */
    const char* (*get_path)(IsoFileSource *src);

    /**
     * Get the name of the file, with the dir component of the path. 
     * 
     * @return
     *     the name of the file, it should be freed when no more needed.
     */
    char* (*get_name)(IsoFileSource *src);

    /**
     * Get information about the file.
     * @return
     *    1 success, < 0 error
     *      Error codes:
     *         ISO_FILE_ACCESS_DENIED
     *         ISO_FILE_BAD_PATH
     *         ISO_FILE_DOESNT_EXIST
     *         ISO_MEM_ERROR
     *         ISO_FILE_ERROR
     *         ISO_NULL_POINTER
     */
    int (*lstat)(IsoFileSource *src, struct stat *info);

    /**
     * Get information about the file. If the file is a symlink, the info
     * returned refers to the destination.
     * 
     * @return
     *    1 success, < 0 error
     *      Error codes:
     *         ISO_FILE_ACCESS_DENIED
     *         ISO_FILE_BAD_PATH
     *         ISO_FILE_DOESNT_EXIST
     *         ISO_MEM_ERROR
     *         ISO_FILE_ERROR
     *         ISO_NULL_POINTER
     */
    int (*stat)(IsoFileSource *src, struct stat *info);

    /**
     * Opens the source.
     * @return 1 on success, < 0 on error
     *      Error codes:
     *         ISO_FILE_ALREADY_OPENNED
     *         ISO_FILE_ACCESS_DENIED
     *         ISO_FILE_BAD_PATH
     *         ISO_FILE_DOESNT_EXIST
     *         ISO_MEM_ERROR
     *         ISO_FILE_ERROR
     *         ISO_NULL_POINTER
     */
    int (*open)(IsoFileSource *src);

    /**
     * Close a previuously openned file
     * @return 1 on success, < 0 on error
     *      Error codes:
     *         ISO_FILE_ERROR
     *         ISO_NULL_POINTER
     *         ISO_FILE_NOT_OPENNED
     */
    int (*close)(IsoFileSource *src);

    /**
     * Attempts to read up to count bytes from the given source into
     * the buffer starting at buf.
     * 
     * The file src must be open() before calling this, and close() when no 
     * more needed. Not valid for dirs. On symlinks it reads the destination
     * file.
     * 
     * @return 
     *     number of bytes read, 0 if EOF, < 0 on error
     *      Error codes:
     *         ISO_FILE_ERROR
     *         ISO_NULL_POINTER
     *         ISO_FILE_NOT_OPENNED
     *         ISO_FILE_IS_DIR
     *         ISO_MEM_ERROR
     *         ISO_INTERRUPTED
     */
    int (*read)(IsoFileSource *src, void *buf, size_t count);

    /**
     * Read a directory. 
     * 
     * Each call to this function will return a new children, until we reach
     * the end of file (i.e, no more children), in that case it returns 0.
     * 
     * The dir must be open() before calling this, and close() when no more
     * needed. Only valid for dirs. 
     * 
     * Note that "." and ".." children MUST NOT BE returned.
     * 
     * @param child
     *     pointer to be filled with the given child. Undefined on error or OEF
     * @return 
     *     1 on success, 0 if EOF (no more children), < 0 on error
     *      Error codes:
     *         ISO_FILE_ERROR
     *         ISO_NULL_POINTER
     *         ISO_FILE_NOT_OPENNED
     *         ISO_FILE_IS_NOT_DIR
     *         ISO_MEM_ERROR
     */
    int (*readdir)(IsoFileSource *src, IsoFileSource **child);

    /**
     * Read the destination of a symlink. You don't need to open the file
     * to call this.
     * 
     * @param buf 
     *     allocated buffer of at least bufsiz bytes. 
     *     The dest. will be copied there, and it will be NULL-terminated
     * @param bufsiz
     *     characters to be copied. Destination link will be truncated if
     *     it is larger than given size. This include the \0 character.
     * @return 
     *     1 on success, < 0 on error
     *      Error codes:
     *         ISO_FILE_ERROR
     *         ISO_NULL_POINTER
     *         ISO_WRONG_ARG_VALUE -> if bufsiz <= 0
     *         ISO_FILE_IS_NOT_SYMLINK
     *         ISO_MEM_ERROR
     *         ISO_FILE_BAD_PATH
     *         ISO_FILE_DOESNT_EXIST
     * 
     */
    int (*readlink)(IsoFileSource *src, char *buf, size_t bufsiz);

    /**
     * Get the filesystem for this source. No extra ref is added, so you
     * musn't unref the IsoFilesystem.
     * 
     * @return
     *     The filesystem, NULL on error
     */
    IsoFilesystem* (*get_filesystem)(IsoFileSource *src);

    /**
     * Free implementation specific data. Should never be called by user.
     * Use iso_file_source_unref() instead.
     */
    void (*free)(IsoFileSource *src);

    /*
     * TODO #00004 Add a get_mime_type() function.
     * This can be useful for GUI apps, to choose the icon of the file
     */
} IsoFileSourceIface;

struct Iso_File_Source
{
    const IsoFileSourceIface *class;
    int refcount;
    void *data;
};

void iso_file_source_ref(IsoFileSource *src);
void iso_file_source_unref(IsoFileSource *src);

/* 
 * this are just helpers to invoque methods in class
 */
extern inline
const char* iso_file_source_get_path(IsoFileSource *src)
{
    return src->class->get_path(src);
}

extern inline
char* iso_file_source_get_name(IsoFileSource *src)
{
    return src->class->get_name(src);
}

extern inline
int iso_file_source_lstat(IsoFileSource *src, struct stat *info)
{
    return src->class->lstat(src, info);
}

extern inline
int iso_file_source_stat(IsoFileSource *src, struct stat *info)
{
    return src->class->stat(src, info);
}

extern inline
int iso_file_source_open(IsoFileSource *src)
{
    return src->class->open(src);
}

extern inline
int iso_file_source_close(IsoFileSource *src)
{
    return src->class->close(src);
}

extern inline
int iso_file_source_read(IsoFileSource *src, void *buf, size_t count)
{
    return src->class->read(src, buf, count);
}

extern inline
int iso_file_source_readdir(IsoFileSource *src, IsoFileSource **child)
{
    return src->class->readdir(src, child);
}

extern inline
int iso_file_source_readlink(IsoFileSource *src, char *buf, size_t bufsiz)
{
    return src->class->readlink(src, buf, bufsiz);
}

extern inline
IsoFilesystem* iso_file_source_get_filesystem(IsoFileSource *src)
{
    return src->class->get_filesystem(src);
}

/**
 * Create a new IsoFileSource from a local filesystem path.
 * While this is usually called by corresponding method in IsoFilesystem
 * object, for local filesystem it is legal to call this directly.
 */
int iso_file_source_new_lfs(const char *path, IsoFileSource **src);

void iso_filesystem_ref(IsoFilesystem *fs);
void iso_filesystem_unref(IsoFilesystem *fs);

/**
 * Create a new IsoFilesystem to deal with local filesystem.
 * 
 * @return
 *     1 sucess, < 0 error
 */
int iso_local_filesystem_new(IsoFilesystem **fs);

#endif /*LIBISO_FSOURCE_H_*/
