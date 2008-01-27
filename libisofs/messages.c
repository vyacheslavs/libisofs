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
int abort_threshold = LIBISO_MSGS_SEV_FAILURE;

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

int iso_set_abort_severity(char *severity)
{
    int ret, sevno;

    ret = libiso_msgs__text_to_sev(severity, &sevno, 0);
    if (ret <= 0)
        return ISO_WRONG_ARG_VALUE;
    if (sevno > LIBISO_MSGS_SEV_FAILURE || sevno < LIBISO_MSGS_SEV_NOTE) 
        return ISO_WRONG_ARG_VALUE;
    ret = abort_threshold;
    abort_threshold = sevno;
    return ret;
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
    switch(errcode) {
    case ISO_CANCELED: 
        return "Operation canceled";
    case ISO_FATAL_ERROR: 
        return "Unknown or unexpected fatal error";
    case ISO_ERROR: 
        return "Unknown or unexpected error";
    case ISO_ASSERT_FAILURE: 
        return "Internal programming error. Please report this bug";
    case ISO_NULL_POINTER: 
        return "NULL pointer as value for an arg. that doesn't allow NULL";
    case ISO_OUT_OF_MEM: 
        return "Memory allocation error";
    case ISO_INTERRUPTED: 
        return "Interrupted by a signal";
    case ISO_WRONG_ARG_VALUE: 
        return "Invalid parameter value";
    case ISO_THREAD_ERROR: 
        return "Can't create a needed thread";
    case ISO_WRITE_ERROR: 
        return "Write error";
    case ISO_BUF_READ_ERROR: 
        return "Buffer read error";
    case ISO_NODE_ALREADY_ADDED: 
        return "Trying to add to a dir a node already added to a dir";
    case ISO_NODE_NAME_NOT_UNIQUE: 
        return "Node with same name already exists";
    case ISO_NODE_NOT_ADDED_TO_DIR: 
        return "Trying to remove a node that was not added to dir";
    case ISO_NODE_DOESNT_EXIST: 
        return "A requested node does not exists";
    case ISO_IMAGE_ALREADY_BOOTABLE: 
        return "Try to set the boot image of an already bootable image";
    case ISO_BOOT_IMAGE_NOT_VALID: 
        return "Trying to use an invalid file as boot image";
    case ISO_FILE_ERROR: 
        return "Error on file operation";
    case ISO_FILE_ALREADY_OPENNED: 
        return "Trying to open an already openned file";
    case ISO_FILE_ACCESS_DENIED: 
        return "Access to file is not allowed";
    case ISO_FILE_BAD_PATH: 
        return "Incorrect path to file";
    case ISO_FILE_DOESNT_EXIST: 
        return "The file does not exists in the filesystem";
    case ISO_FILE_NOT_OPENNED: 
        return "Trying to read or close a file not openned";
    case ISO_FILE_IS_DIR: 
        return "Directory used where no dir is expected";
    case ISO_FILE_READ_ERROR: 
        return "Read error";
    case ISO_FILE_IS_NOT_DIR: 
        return "Not dir used where a dir is expected";
    case ISO_FILE_IS_NOT_SYMLINK: 
        return "Not symlink used where a symlink is expected";
    case ISO_FILE_SEEK_ERROR: 
        return "Can't seek to specified location";
    case ISO_FILE_IGNORED: 
        return "File not supported in ECMA-119 tree and thus ignored";
    case ISO_FILE_TOO_BIG: 
        return "A file is bigger than supported by used standard";
    case ISO_FILE_CANT_WRITE: 
        return "File read error during image creations";
    case ISO_FILENAME_WRONG_CHARSET: 
        return "Can't convert filename to requested charset";
    case ISO_FILE_CANT_ADD: 
        return "File can't be added to the tree";
    case ISO_FILE_IMGPATH_WRONG: 
        return "File path break specification constraints and will be ignored";
    case ISO_CHARSET_CONV_ERROR: 
        return "Charset conversion error";
    case ISO_MANGLE_TOO_MUCH_FILES: 
        return "Too much files to mangle, can't guarantee unique file names";
    case ISO_WRONG_PVD: 
        return "Wrong or damaged Primary Volume Descriptor";
    case ISO_WRONG_RR: 
        return "Wrong or damaged RR entry";
    case ISO_UNSUPPORTED_RR: 
        return "Unsupported RR feature";
    case ISO_WRONG_ECMA119: 
        return "Wrong or damaged ECMA-119";
    case ISO_UNSUPPORTED_ECMA119: 
        return "Unsupported ECMA-119 feature";
    case ISO_WRONG_EL_TORITO: 
        return "Wrong or damaged El-Torito catalog";
    case ISO_UNSUPPORTED_EL_TORITO: 
        return "Unsupported El-Torito feature";
    case ISO_ISOLINUX_CANT_PATCH: 
        return "Can't patch isolinux boot image";
    case ISO_UNSUPPORTED_SUSP: 
        return "Unsupported SUSP feature";
    case ISO_WRONG_RR_WARN: 
        return "Error on a RR entry that can be ignored";
    case ISO_SUSP_UNHANDLED: 
        return "Error on a RR entry that can be ignored";
    case ISO_SUSP_MULTIPLE_ER: 
        return "Multiple ER SUSP entries found";
    case ISO_UNSUPPORTED_VD: 
        return "Unsupported volume descriptor found";
    case ISO_EL_TORITO_WARN: 
        return "El-Torito related warning";
    default:
        return "Unknown error";
    }
}

int iso_msg_submit(int imgid, int errcode, int causedby, const char *fmt, ...)
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
    if (causedby != 0) {
        iso_msg_debug(imgid, " > Caused by: %s", iso_error_to_msg(causedby));
        if (ISO_ERR_SEV(causedby) == LIBISO_MSGS_SEV_FATAL) {
            return ISO_CANCELED;
        }
    }

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
int iso_obtain_msgs(char *minimum_severity, int *error_code, int *imgid,
                    char msg_text[], int *os_errno, char severity[])
{
    int ret, minimum_sevno, sevno, priority;
    double timestamp;
    pid_t pid;
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


    ret = libiso_msgs_item_get_origin(item, &timestamp, &pid, imgid, 0);
    if (ret <= 0)
        goto ex;
    
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
