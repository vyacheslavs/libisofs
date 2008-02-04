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

/**
 * Take and increment this variable to get a valid identifier for message
 * origin.
 */
extern int iso_message_id;

/**
 * Submit a debug message.
 */
void iso_msg_debug(int imgid, const char *fmt, ...);

/**
 * 
 * @param errcode
 *      The error code.
 * @param causedby
 *      Error that was caused the errcode. If this error is a FATAL error,
 *      < 0 will be returned in any case. Use 0 if there is no previous 
 *      cause for the error.
 * @return
 *      1 on success, < 0 if function must abort asap.
 */
int iso_msg_submit(int imgid, int errcode, int causedby, const char *fmt, ...);

#endif /*MESSAGES_H_*/
