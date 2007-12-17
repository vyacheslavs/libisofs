/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "libisofs.h"
#include "messages.h"
#include "image.h"

#define MAX_MSG_LEN     4096

void iso_msg_debug(IsoImage *img, const char *fmt, ...)
{
    char msg[MAX_MSG_LEN];
    va_list ap;
    
    va_start(ap, fmt);
    vsnprintf(msg, MAX_MSG_LEN, fmt, ap);
    va_end(ap);
    
	libiso_msgs_submit(img->messenger, -1, 0x00000002, LIBISO_MSGS_SEV_DEBUG, 
                       LIBISO_MSGS_PRIO_ZERO, msg, 0, 0);
}

void iso_msg_note(IsoImage *img, int error_code, const char *fmt, ...)
{
	char msg[MAX_MSG_LEN];
    va_list ap;
    
    va_start(ap, fmt);
    vsnprintf(msg, MAX_MSG_LEN, fmt, ap);
    va_end(ap);
    
    libiso_msgs_submit(img->messenger, -1, error_code, LIBISO_MSGS_SEV_NOTE, 
                       LIBISO_MSGS_PRIO_MEDIUM, msg, 0, 0);
}

void iso_msg_hint(IsoImage *img, int error_code, const char *fmt, ...)
{
	char msg[MAX_MSG_LEN];
    va_list ap;
    
    va_start(ap, fmt);
    vsnprintf(msg, MAX_MSG_LEN, fmt, ap);
    va_end(ap);
    
    libiso_msgs_submit(img->messenger, -1, error_code, LIBISO_MSGS_SEV_HINT, 
                       LIBISO_MSGS_PRIO_MEDIUM, msg, 0, 0);
}

void iso_msg_warn(IsoImage *img, int error_code, const char *fmt, ...)
{
	char msg[MAX_MSG_LEN];
    va_list ap;
    
    va_start(ap, fmt);
    vsnprintf(msg, MAX_MSG_LEN, fmt, ap);
    va_end(ap);
    
    libiso_msgs_submit(img->messenger, -1, error_code,
                       LIBISO_MSGS_SEV_WARNING, LIBISO_MSGS_PRIO_MEDIUM, 
                       msg, 0, 0);
}

void iso_msg_sorry(IsoImage *img, int error_code, const char *fmt, ...)
{
	char msg[MAX_MSG_LEN];
    va_list ap;
    
    va_start(ap, fmt);
    vsnprintf(msg, MAX_MSG_LEN, fmt, ap);
    va_end(ap);
    
    libiso_msgs_submit(img->messenger, -1, error_code,
                       LIBISO_MSGS_SEV_SORRY, LIBISO_MSGS_PRIO_HIGH, 
                       msg, 0, 0);
}

void iso_msg_fatal(IsoImage *img, int error_code, const char *fmt, ...)
{
	char msg[MAX_MSG_LEN];
    va_list ap;
    
    va_start(ap, fmt);
    vsnprintf(msg, MAX_MSG_LEN, fmt, ap);
    va_end(ap);
    
    libiso_msgs_submit(img->messenger, -1, error_code,
                       LIBISO_MSGS_SEV_FATAL, LIBISO_MSGS_PRIO_HIGH, 
                       msg, 0, 0);
}

/** 
 * Control queueing and stderr printing of messages from libisofs.
 * Severity may be one of "NEVER", "FATAL", "SORRY", "WARNING", "HINT",
 * "NOTE", "UPDATE", "DEBUG", "ALL".
 * 
 * @param queue_severity Gives the minimum limit for messages to be queued.
 *                       Default: "NEVER". If you queue messages then you
 *                       must consume them by iso_msgs_obtain().
 * @param print_severity Does the same for messages to be printed directly
 *                       to stderr.
 * @param print_id       A text prefix to be printed before the message.
 * @return               >0 for success, <=0 for error
 */
int iso_image_set_msgs_severities(IsoImage *img, char *queue_severity,
                                  char *print_severity, char *print_id)
{
	int ret, queue_sevno, print_sevno;

	ret = libiso_msgs__text_to_sev(queue_severity, &queue_sevno, 0);
	if (ret <= 0)
		return 0;
	ret = libiso_msgs__text_to_sev(print_severity, &print_sevno, 0);
	if (ret <= 0)
		return 0;
	ret = libiso_msgs_set_severities(img->messenger, queue_sevno,
					 print_sevno, print_id, 0);
	if (ret <= 0)
		return 0;
	return 1;
}

#define ISO_MSGS_MESSAGE_LEN 4096

/** 
 * Obtain the oldest pending libisofs message from the queue which has at
 * least the given minimum_severity. This message and any older message of
 * lower severity will get discarded from the queue and is then lost forever.
 * 
 * Severity may be one of "NEVER", "FATAL", "SORRY", "WARNING", "HINT",
 * "NOTE", "UPDATE", "DEBUG", "ALL". To call with minimum_severity "NEVER"
 * will discard the whole queue.
 * 
 * @param error_code Will become a unique error code as listed in messages.h
 * @param msg_text   Must provide at least ISO_MSGS_MESSAGE_LEN bytes.
 * @param os_errno   Will become the eventual errno related to the message
 * @param severity   Will become the severity related to the message and
 *                   should provide at least 80 bytes.
 * @return 1 if a matching item was found, 0 if not, <0 for severe errors
 */
int iso_image_obtain_msgs(IsoImage *img, char *minimum_severity,
                     int *error_code, char msg_text[], int *os_errno,
                     char severity[])
{
	int ret, minimum_sevno, sevno, priority;
	char *textpt, *sev_name;
	struct libiso_msgs_item *item = NULL;

	if (img == NULL)
		return 0;

	ret = libiso_msgs__text_to_sev(minimum_severity, &minimum_sevno, 0);
	if (ret <= 0)
		return 0;
	ret = libiso_msgs_obtain(img->messenger, &item, minimum_sevno,
				LIBISO_MSGS_PRIO_ZERO, 0);
	if (ret <= 0)
		goto ex;
	ret = libiso_msgs_item_get_msg(item, error_code, &textpt, os_errno, 0);
	if (ret <= 0)
		goto ex;
	strncpy(msg_text, textpt, ISO_MSGS_MESSAGE_LEN-1);
	if(strlen(textpt) >= ISO_MSGS_MESSAGE_LEN)
		msg_text[ISO_MSGS_MESSAGE_LEN-1] = 0;

	severity[0]= 0;
	ret = libiso_msgs_item_get_rank(item, &sevno, &priority, 0);
	if(ret <= 0)
		goto ex;
	ret = libiso_msgs__sev_to_text(sevno, &sev_name, 0);
	if(ret <= 0)
		goto ex;
	strcpy(severity,sev_name);

	ret = 1;
ex:
	libiso_msgs_destroy_item(img->messenger, &item, 0);
	return ret;
}

void *iso_image_get_messenger(IsoImage *img)
{
	return img->messenger;
}

