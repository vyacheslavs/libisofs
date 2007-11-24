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
 * Some functions here will be moved to libisofs.h when we expose 
 * Sources.
 */

#include <sys/stat.h>

typedef struct Iso_File_Source IsoFileSource;
typedef struct Iso_Filesystem IsoFilesystem;

struct Iso_Filesystem
{

    /**
     * 
     * @return
     *    1 on success, < 0 on error
     */
    int (*get_root)(IsoFilesystem *fs, IsoFileSource *root);


};

struct Iso_File_Source
{

    /**
     * 
     * @return
     * 		1 success, < 0 error
     */
    int (*lstat)(IsoFileSource *src, struct stat *info);

    //stat?

    /**
     * Opens the source.
     * @return 1 on success, < 0 on error
     */
    int (*open)(IsoFileSource *src);


    void (*close)(IsoFileSource *src);

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
     * @param child
     *     pointer to be filled with the given child. Undefined on error or OEF
     * @return 
     *     1 on success, 0 if EOF (no more children), < 0 on error
     */
    int (*readdir)(IsoFileSource *src, IsoFileSource **child);

    /**
     * 
     */
    int (*readlink)(IsoFileSource *src, char *buf, size_t bufsiz);

};


#endif /*LIBISO_FSOURCE_H_*/
