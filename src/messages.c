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
#include "error.h"

int iso_message_id = LIBISO_MSGS_ORIGIN_IMAGE_BASE;

/**
 * Threshold for aborting.
 */
int abort_threshold = LIBISO_MSGS_SEV_ERROR;

#define MAX_MSG_LEN     4096

struct libiso_msgs *libiso_msgr = NULL;

int iso_init()
{
    if (libiso_msgr == NULL) {
        if (libiso_msgs_new(&libiso_msgr, 0) <= 0)
            return ISO_FATAL_ERROR;
    }
    libiso_msgs_set_severities(libiso_msgr, LIBISO_MSGS_SEV_NEVER,
                   LIBISO_MSGS_SEV_FATAL, "libisofs: ", 0);
    return 1;
}

void iso_finish()
{
    libiso_msgs_destroy(&libiso_msgr, 0);
}

void iso_msg_debug(int imgid, const char *fmt, ...)
{
    char msg[MAX_MSG_LEN];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, MAX_MSG_LEN, fmt, ap);
    va_end(ap);

    libiso_msgs_submit(libiso_msgr, imgid, 0x00000002, LIBISO_MSGS_SEV_DEBUG,
                       LIBISO_MSGS_PRIO_ZERO, msg, 0, 0);
}

const char *iso_error_to_msg(int errcode)
{
    /* TODO not implemented yet!!! */
    return "TODO";
}

int iso_msg_submit(int imgid, int errcode, const char *fmt, ...)
{
    char msg[MAX_MSG_LEN];
    va_list ap;
    
    /* when called with ISO_CANCELED, we don't need to submit any message */
    if (errcode == ISO_CANCELED && fmt == NULL) {
        return ISO_CANCELED;
    }

    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(msg, MAX_MSG_LEN, fmt, ap);
        va_end(ap);
    } else {
        strncpy(msg, iso_error_to_msg(errcode), MAX_MSG_LEN);
    }

    libiso_msgs_submit(libiso_msgr, imgid, errcode, ISO_ERR_SEV(errcode),
                       ISO_ERR_PRIO(errcode), msg, 0, 0);
    
    if (ISO_ERR_SEV(errcode) >= abort_threshold) {
        return ISO_CANCELED;
    } else {
        return 0;
    }
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
int iso_set_msgs_severities(char *queue_severity, char *print_severity, 
                            char *print_id)
{
    int ret, queue_sevno, print_sevno;

    ret = libiso_msgs__text_to_sev(queue_severity, &queue_sevno, 0);
    if (ret <= 0)
        return 0;
    ret = libiso_msgs__text_to_sev(print_severity, &print_sevno, 0);
    if (ret <= 0)
        return 0;
    ret = libiso_msgs_set_severities(libiso_msgr, queue_sevno, print_sevno,
                                     print_id, 0);
    if (ret <= 0)
        return 0;
    return 1;
}

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
int iso_obtain_msgs(char *minimum_severity, int *error_code, 
                    char msg_text[], int *os_errno, char severity[])
{
    int ret, minimum_sevno, sevno, priority;
    char *textpt, *sev_name;
    struct libiso_msgs_item *item= NULL;

    ret = libiso_msgs__text_to_sev(minimum_severity, &minimum_sevno, 0);
    if (ret <= 0)
        return 0;
    ret = libiso_msgs_obtain(libiso_msgr, &item, minimum_sevno, 
                             LIBISO_MSGS_PRIO_ZERO, 0);
    if (ret <= 0)
        goto ex;
    ret = libiso_msgs_item_get_msg(item, error_code, &textpt, os_errno, 0);
    if (ret <= 0)
        goto ex;
    strncpy(msg_text, textpt, ISO_MSGS_MESSAGE_LEN-1);
    if (strlen(textpt) >= ISO_MSGS_MESSAGE_LEN)
        msg_text[ISO_MSGS_MESSAGE_LEN-1] = 0;

    severity[0]= 0;
    ret = libiso_msgs_item_get_rank(item, &sevno, &priority, 0);
    if (ret <= 0)
        goto ex;
    ret = libiso_msgs__sev_to_text(sevno, &sev_name, 0);
    if (ret <= 0)
        goto ex;
    strcpy(severity, sev_name);

    ret = 1;
    ex: ;
    libiso_msgs_destroy_item(libiso_msgr, &item, 0);
    return ret;
}

/**
 * Return the messenger object handle used by libisofs. This handle
 * may be used by related libraries to  their own compatible
 * messenger objects and thus to direct their messages to the libisofs
 * message queue. See also: libburn, API function burn_set_messenger().
 * 
 * @return the handle. Do only use with compatible
 */
void *iso_get_messenger()
{
    return libiso_msgr;
}
