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
 * Error codes and return values for libisofs.
 * TODO #00003 make this header public
 */

/* 
 * error codes are 32 bit numbers, that follow the following conventions:
 * 
 * bit  31 (MSB) -> 1 (to make the value always negative)
 * bits 30-24 -> Encoded severity (Use ISO_ERR_SEV to translate an error code
 *               to a LIBISO_MSGS_SEV_* constant)
 *        = 0x10 -> DEBUG
 *        = 0x20 -> UPDATE
 *        = 0x30 -> NOTE
 *        = 0x40 -> HINT
 *        = 0x50 -> WARNING
 *        = 0x60 -> SORRY
 *        = 0x6A -> ERROR
 *        = 0x70 -> FATAL
 *        = 0x71 -> ABORT
 * bits 23-20 -> Encoded priority (Use ISO_ERR_PRIO to translate an error code
 *               to a LIBISO_MSGS_PRIO_* constant)
 *        = 0x0 -> ZERO
 *        = 0x1 -> LOW
 *        = 0x2 -> MEDIUM
 *        = 0x3 -> HIGH
 *        = 0x7 -> TOP
 * bits 19-16 -> Reserved for future usage (maybe message ranges)
 * bits 15-0  -> Error code
 */

#define ISO_ERR_SEV(e)      (e & 0x7F000000)
#define ISO_ERR_PRIO(e)     ((e & 0x00F00000) << 8)
#define ISO_ERR_CODE(e)     (e & 0x0000FFFF)

/** successfully execution */
#define ISO_SUCCESS                     1

/** 
 * special return value, it could be or not an error depending on the 
 * context. 
 */
#define ISO_NONE                        0

/** Operation canceled (ERROR,TOP, -1) */
#define ISO_CANCELED                    0xEA70FFFF

/** Unknown or unexpected fatal error (FATAL,HIGH, -2) */
#define ISO_FATAL_ERROR                 0xF030FFFE

/** Unknown or unexpected error (ERROR,HIGH, -3) */
#define ISO_ERROR                       0xEA30FFFD

/** Internal programming error. Please report this bug (FATAL,HIGH, -4) */
#define ISO_ASSERT_FAILURE              0xF030FFFC

/** 
 * NULL pointer as value for an arg. that doesn't allow NULL (ERROR,HIGH, -5)
 */
#define ISO_NULL_POINTER                0xEA30FFFB

/** Memory allocation error (FATAL,HIGH, -6) */
#define ISO_OUT_OF_MEM                  0xF030FFFA

/** Interrupted by a signal (FATAL,HIGH, -7) */
#define ISO_INTERRUPTED                 0xF030FFF9

/** Invalid parameter value (ERROR,HIGH, -8) */
#define ISO_WRONG_ARG_VALUE             0xEA30FFF8

/** Can't create a needed thread (FATAL,HIGH, -9) */
#define ISO_THREAD_ERROR                0xF030FFF7

/** Write error (ERROR,HIGH, -10) */
#define ISO_WRITE_ERROR                 0xEA30FFF6

/** Buffer read error (ERROR,HIGH, -11) */
#define ISO_BUF_READ_ERROR              0xEA30FFF5

/** Trying to add to a dir a node already added to a dir (ERROR,HIGH, -64) */
#define ISO_NODE_ALREADY_ADDED          0xEA30FFC0

/** Node with same name already exists (ERROR,HIGH, -65) */
#define ISO_NODE_NAME_NOT_UNIQUE        0xEA30FFBF

/** Trying to remove a node that was not added to dir (ERROR,HIGH, -65) */
#define ISO_NODE_NOT_ADDED_TO_DIR       0xEA30FFBE

/** A requested node does not exists  (ERROR,HIGH, -66) */
#define ISO_NODE_DOESNT_EXIST           0xEA30FFBD

/** Try to set the boot image of an already bootable image (ERROR,HIGH, -67) */
#define ISO_IMAGE_ALREADY_BOOTABLE      0xEA30FFBC

/** Trying to use an invalid file as boot image (ERROR,HIGH, -68) */
#define ISO_BOOT_IMAGE_NOT_VALID        0xEA30FFBB

/** 
 * Error on file operation (ERROR,HIGH, -128) 
 * (take a look at more specified error codes below)
 */
#define ISO_FILE_ERROR                  0xEA30FF80

/** Trying to open an already openned file (ERROR,HIGH, -129) */
#define ISO_FILE_ALREADY_OPENNED        0xEA30FF7F

/** Access to file is not allowed (ERROR,HIGH, -130) */
#define ISO_FILE_ACCESS_DENIED          0xEA30FF7E

/** Incorrect path to file (ERROR,HIGH, -131) */
#define ISO_FILE_BAD_PATH               0xEA30FF7D

/** The file does not exists in the filesystem (ERROR,HIGH, -132) */
#define ISO_FILE_DOESNT_EXIST           0xEA30FF7C

/** Trying to read or close a file not openned (ERROR,HIGH, -133) */
#define ISO_FILE_NOT_OPENNED            0xEA30FF7B

/** Directory used where no dir is expected (ERROR,HIGH, -134) */
#define ISO_FILE_IS_DIR                 0xEA30FF7A

/** Read error (ERROR,HIGH, -135) */
#define ISO_FILE_READ_ERROR             0xEA30FF79

/** Not dir used where a dir is expected (ERROR,HIGH, -136) */
#define ISO_FILE_IS_NOT_DIR             0xEA30FF78

/** Not symlink used where a symlink is expected (ERROR,HIGH, -137) */
#define ISO_FILE_IS_NOT_SYMLINK         0xEA30FF77

/** Can't seek to specified location (ERROR,HIGH, -138) */
#define ISO_FILE_SEEK_ERROR             0xEA30FF76

/** File not supported in ECMA-119 tree and thus ignored (HINT,MEDIUM, -139) */
#define ISO_FILE_IGNORED                0xC020FF75

/* A file is bigger than supported by used standard  (HINT,MEDIUM, -140) */
#define ISO_FILE_TOO_BIG                0xC020FF74

/* File read error during image creations (SORRY,HIGH, -141) */
#define ISO_FILE_CANT_WRITE             0xE030FF73

/* Can't convert filename to requested charset (HINT,MEDIUM, -142) */
#define ISO_FILENAME_WRONG_CHARSET      0xC020FF72

/* File can't be added to the tree (WARNING,MEDIUM, -143) */
#define ISO_FILE_CANT_ADD               0xD020FF71

/** 
 * File path break specification constraints and will be ignored 
 * (HINT,MEDIUM, -141) 
 */
#define ISO_FILE_IMGPATH_WRONG          0xC020FF73

/** Charset conversion error (ERROR,HIGH, -256) */
#define ISO_CHARSET_CONV_ERROR          0xEA30FF00

/** 
 * Too much files to mangle, i.e. we cannot guarantee unique file names 
 * (ERROR,HIGH, -257) 
 */
#define ISO_MANGLE_TOO_MUCH_FILES       0xEA30FEFF

/* image related errors */

/** 
 * Wrong or damaged Primary Volume Descriptor (ERROR,HIGH, -320)
 * This could mean that the file is not a valid ISO image. 
 */
#define ISO_WRONG_PVD                   0xEA30FEC0

/** Wrong or damaged RR entry (SORRY,HIGH, -321) */
#define ISO_WRONG_RR                    0xE030FEBF

/** Unsupported RR feature (SORRY,HIGH, -322) */
#define ISO_UNSUPPORTED_RR              0xE030FEBE

/** Wrong or damaged ECMA-119 (ERROR,HIGH, -323) */
#define ISO_WRONG_ECMA119               0xEA30FEBD

/** Unsupported ECMA-119 feature (ERROR,HIGH, -324) */
#define ISO_UNSUPPORTED_ECMA119         0xEA30FEBC

/** Wrong or damaged El-Torito catalog (SORRY,HIGH, -325) */
#define ISO_WRONG_EL_TORITO             0xE030FEBB

/** Unsupported El-Torito feature (SORRY,HIGH, -326) */
#define ISO_UNSUPPORTED_EL_TORITO       0xE030FEBA

/** Can't patch an isolinux boot image (SORRY,HIGH, -327) */
#define ISO_ISOLINUX_CANT_PATCH         0xE030FEB9

/** Unsupported SUSP feature (SORRY,HIGH, -328) */
#define ISO_UNSUPPORTED_SUSP            0xE030FEB8

/** Error on a RR entry that can be ignored (WARNING,MEDIUM, -329) */
#define ISO_WRONG_RR_WARN               0xD020FEB7

/** Error on a RR entry that can be ignored (HINT,MEDIUM, -330) */
#define ISO_SUSP_UNHANDLED              0xC020FEB6

/** Multiple ER SUSP entries found (WARNING,MEDIUM, -331) */
#define ISO_SUSP_MULTIPLE_ER            0xD020FEB5

/** Unsupported volume descriptor found (HINT,MEDIUM, -332) */
#define ISO_UNSUPPORTED_VD              0xC020FEB4

/** El-Torito related warning (WARNING,MEDIUM, -333) */
#define ISO_EL_TORITO_WARN              0xD020FEB3

#endif /*LIBISO_ERROR_H_*/
