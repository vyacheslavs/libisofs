/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_ERROR_H_
#define LIBISO_ERROR_H_

/*
 * Return values for libisofs functions, mainly error codes
 * TODO #00003 make this header public
 */

#define ISO_SUCCESS                     1
#define ISO_ERROR                       -1
#define ISO_NULL_POINTER                -2
#define ISO_OUT_OF_MEM                  -3
#define ISO_MEM_ERROR                   -4
#define ISO_INTERRUPTED                 -5
#define ISO_WRONG_ARG_VALUE             -6

#define ISO_WRITE_ERROR                 -10
#define ISO_THREAD_ERROR                -11

#define ISO_NODE_ALREADY_ADDED          -50
#define ISO_NODE_NAME_NOT_UNIQUE        -51
#define ISO_NODE_NOT_ADDED_TO_DIR       -52

#define ISO_FILE_ERROR                  -100
#define ISO_FILE_ALREADY_OPENNED        -101
#define ISO_FILE_ACCESS_DENIED          -102
#define ISO_FILE_BAD_PATH               -103
#define ISO_FILE_DOESNT_EXIST           -104
#define ISO_FILE_NOT_OPENNED            -105
#define ISO_FILE_IS_DIR                 -106
#define ISO_FILE_READ_ERROR             -107
#define ISO_FILE_IS_NOT_DIR             -108
#define ISO_FILE_IS_NOT_SYMLINK         -109
#define ISO_FILE_TOO_BIG                -110
#define ISO_FILE_SEEK_ERROR             -111

#define ISO_CHARSET_CONV_ERROR          -150

#define ISO_MANGLE_TOO_MUCH_FILES       -200

/* image read errors */
#define ISO_WRONG_PVD                   -300
#define ISO_WRONG_RR                    -301
#define ISO_UNSUPPORTED_RR              -302
#define ISO_WRONG_ECMA119               -303
#define ISO_UNSUPPORTED_ECMA119         -304
#define ISO_WRONG_EL_TORITO             -305

/* el-torito errors */
#define ISO_IMAGE_ALREADY_BOOTABLE      -350
#define ISO_BOOT_IMAGE_NOT_VALID        -351


#endif /*LIBISO_ERROR_H_*/
