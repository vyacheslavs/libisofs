/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

/*
 * Message handling for libisofs
 */

#ifndef MESSAGES_H_
#define MESSAGES_H_

#include "libiso_msgs.h"

/**
 * Take and increment this variable to get a valid identifier for message
 * origin.
 */
extern int iso_message_id;

/** File cannot be added to image (ignored) */
#define LIBISO_FILE_IGNORED         0x00030100
/** File cannot be writing to image (ignored) */
#define LIBISO_FILE_CANT_WRITE      0x00030101
/** Read error */
#define LIBISO_READ_ERROR           0x00030102
/** Write error */
#define LIBISO_WRITE_ERROR          0x00030103
/** Cannot create writer thread */
#define LIBISO_THREAD_ERROR         0x00030110

/** Charset conversion error */
#define LIBISO_CHARSET_ERROR        0x00030500

/** Can't read file (ignored) */
#define LIBISO_CANT_READ_FILE		0x00031001
/** Can't read file (operation canceled) */
#define LIBISO_FILE_READ_ERROR		0x00031002
/** File doesn't exist */
#define LIBISO_FILE_DOESNT_EXIST	0x00031003
/** Read access denied */
#define LIBISO_FILE_NO_READ_ACCESS	0x00031004

/** Unsupported image feature */
#define LIBISO_IMG_UNSUPPORTED		0x00031000
/** Unsupported Vol Desc that will be ignored */
#define LIBISO_UNSUPPORTED_VD		0x00031001
/** damaged image */
#define LIBISO_WRONG_IMG			0x00031002
/** Can't read previous image file */
#define LIBISO_CANT_READ_IMG		0x00031003

/* Unsupported SUSP entry */
#define LIBISO_SUSP_UNHANLED		0x00030101
/* Wrong SUSP entry, that cause RR to be ignored */
#define LIBISO_SUSP_WRONG			0x00030102
/* Unsupported multiple SUSP ER entries where found */
#define LIBISO_SUSP_MULTIPLE_ER		0x00030103

/** Unsupported RR feature. */
#define LIBISO_RR_UNSUPPORTED		0x00030111
/** Error in a Rock Ridge entry. */
#define LIBISO_RR_ERROR				0x00030112
/* Wrong/Damaged Rock Ridge */
#define LIBISO_RR_WARNING           0x00030113

/** Unsupported boot vol desc. */
#define LIBISO_BOOT_VD_UNHANLED			0x00030201
/** Wrong or damaged el-torito catalog */
#define LIBISO_EL_TORITO_WRONG			0x00030202
/** Unsupproted el-torito feature */
#define LIBISO_EL_TORITO_UNHANLED		0x00030203
/** Trying to add an invalid file as a El-Torito image */
#define LIBISO_EL_TORITO_WRONG_IMG		0x00030204
/** Can't properly patch isolinux image */
#define LIBISO_ISOLINUX_CANT_PATCH		0x00030205
/** Copying El-Torito from a previous image without enought info about it */
#define LIBISO_EL_TORITO_BLIND_COPY		0x00030206

/** Unsupported file type for Joliet tree */
#define LIBISO_JOLIET_WRONG_FILE_TYPE	0x00030301

/**
 * Submit a debug message.
 */
void iso_msg_debug(int imgid, const char *fmt, ...);

/**
 * Get a textual description of an error.
 */
const char *iso_error_to_msg(int errcode);

/**
 * TODO add caused by!!
 * 
 * @return
 *      1 on success, < 0 if function must abort asap.
 */
int iso_msg_submit(int imgid, int errcode, const char *fmt, ...);

#endif /*MESSAGES_H_*/
