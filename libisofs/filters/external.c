/*
 * Copyright (c) 2009 Thomas Schmitt
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 *
 * It implements a filter facility which can pipe a IsoStream into an external
 * process, read its output and forward it as IsoStream output to an IsoFile.
 * The external processes get started according to an IsoExternalFilterCommand
 * which is described in libisofs.h.
 * 
 */

#include "../libisofs.h"
#include "../filter.h"
#include "../fsource.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>


/*
 * A filter that starts an external process and uses its stdin and stdout
 * for classical pipe filtering.
 */


/*
 * Individual runtime properties exist only as long as the stream is opened.
 */
typedef struct
{
    int send_fd;
    int recv_fd;
    pid_t pid;
    off_t in_counter;
    int in_eof;
    off_t out_counter;
    int out_eof;
    uint8_t pipebuf[2048]; /* buffers in case of EAGAIN on write() */
    int pipebuf_fill;
    int is_0_run;
} ExternalFilterRuntime;


static
int extf_running_new(ExternalFilterRuntime **running, int send_fd, int recv_fd,
                     pid_t child_pid, int flag)
{
    ExternalFilterRuntime *o;
    *running = o = calloc(sizeof(ExternalFilterRuntime), 1);
    if (o == NULL) {
        return ISO_OUT_OF_MEM;
    }
    o->send_fd = send_fd;
    o->recv_fd = recv_fd;
    o->pid = child_pid;
    o->in_counter = 0;
    o->in_eof = 0;
    o->out_counter = 0;
    o->out_eof = 0;
    memset(o->pipebuf, 0, sizeof(o->pipebuf));
    o->pipebuf_fill = 0;
    o->is_0_run = 0;
    return 1;
}


/*
 * The data payload of an individual IsoStream from External Filter
 */
typedef struct
{
    ino_t id;

    IsoStream *orig;

    IsoExternalFilterCommand *cmd;

    off_t size; /* -1 means that the size is unknown yet */

    ExternalFilterRuntime *running; /* is non-NULL when open */

} ExternalFilterStreamData;


/* Each individual ExternalFilterStreamData needs a unique id number. */
/* >>> This is very suboptimal:
       The counter can rollover.
*/
static ino_t extf_ino_id = 0;


/*
 * Methods for the IsoStreamIface of an External Filter object.
 */


/*
 * @param flag  bit0= do not run .get_size() if size is < 0
 */
static
int extf_stream_open_flag(IsoStream *stream, int flag)
{
    ExternalFilterStreamData *data;
    ExternalFilterRuntime *running = NULL;
    pid_t child_pid;
    int send_pipe[2], recv_pipe[2], ret, stream_open = 0;

    send_pipe[0] = send_pipe[1] = recv_pipe[0] = recv_pipe[1] = -1;

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = (ExternalFilterStreamData*)stream->data;

    if (data->running != NULL) {
        return ISO_FILE_ALREADY_OPENED;
    }
    if (data->cmd->behavior & 1) {
        if (iso_stream_get_size(data->orig) == 0) {
            /* Do not fork. Place message for .read and .close */;
            ret = extf_running_new(&running, -1, -1, 0, 0);
            if (ret < 0) {
                return ret;
            }
            running->is_0_run = 1;
            data->running = running;
            return 1;
        }
    }
    if (data->size < 0 && !(flag & 1)) {
        /* Do the size determination run now, so that the size gets cached
           and .get_size() will not fail on an opened stream.
         */
        stream->class->get_size(stream);
    }
    ret = iso_stream_open(data->orig);
    if (ret < 0) {
        return ret;
    }
    stream_open = 1;

    ret = pipe(send_pipe);
    if (ret == -1) {
        ret = ISO_OUT_OF_MEM;
        goto parent_failed;
    }
    ret = pipe(recv_pipe);
    if (ret == -1) {
        ret = ISO_OUT_OF_MEM;
        goto parent_failed;
    }

    child_pid= fork();
    if (child_pid == -1) {
        ret = ISO_DATA_SOURCE_FATAL;
        goto parent_failed;
    }

    if (child_pid != 0) {
        /* parent */

#ifndef NIX
        ret = extf_running_new(&running, send_pipe[1], recv_pipe[0], child_pid,
                               0);
        if (ret < 0) {
            goto parent_failed;
        }
#else
        running = calloc(sizeof(ExternalFilterRuntime), 1);
        if (running == NULL) {
            ret = ISO_OUT_OF_MEM;
            goto parent_failed;
        }
        running->send_fd = send_pipe[1];
        running->recv_fd = recv_pipe[0];
        running->pid = child_pid;
        running->in_counter = 0;
        running->in_eof = 0;
        running->out_counter = 0;
        running->out_eof = 0;
        memset(running->pipebuf, 0, sizeof(running->pipebuf));
        running->pipebuf_fill = 0;
        running->is_0_run = 0;
#endif /* NIX */

        data->running = running;

        /* Give up the child-side pipe ends */
        close(send_pipe[0]);
        close(recv_pipe[1]);

        /* >>> ??? should one replace non-blocking read() by select () ? */

        /* Make filter outlet non-blocking */
        ret = fcntl(recv_pipe[0], F_GETFL);
        if (ret != -1) {
            ret |= O_NONBLOCK;
            fcntl(recv_pipe[0], F_SETFL, ret);
        }
        /* Make filter sink non-blocking */
        ret = fcntl(send_pipe[1], F_GETFL);
        if (ret != -1) {
            ret |= O_NONBLOCK;
            fcntl(send_pipe[1], F_SETFL, ret);
        }

        return 1;
    }

    /* child */

    /* Give up the parent-side pipe ends */
    close(send_pipe[1]);
    close(recv_pipe[0]);

    /* attach pipe ends to stdin and stdout */;
    close(0);
    ret = dup2(send_pipe[0], 0);
    if (ret == -1) {
        goto child_failed;
    }
    close(1);
    ret = dup2(recv_pipe[1], 1);
    if (ret == -1) {
        goto child_failed;
    }

    /* Self conversion into external program */
    execv(data->cmd->path, data->cmd->argv); /* should never come back */

child_failed:;
    fprintf(stderr,"--- execution of external filter command failed:\n");
    fprintf(stderr,"    %s\n", data->cmd->path);
    exit(127);

parent_failed:;
    if (stream_open)
        iso_stream_close(data->orig);
    if(send_pipe[0] != -1)
        close(send_pipe[0]);
    if(send_pipe[1] != -1)
        close(send_pipe[1]);
    if(recv_pipe[0] != -1)
        close(recv_pipe[0]);
    if(recv_pipe[1] != -1)
        close(recv_pipe[1]);
    return ret;
}


static
int extf_stream_open(IsoStream *stream)
{
    return extf_stream_open_flag(stream, 0);
}


static
int extf_stream_close(IsoStream *stream)
{
    int ret, status, is_0_run;
    ExternalFilterStreamData *data;

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = stream->data;

    if (data->running == NULL) {
        return 1;
    }
    is_0_run = data->running->is_0_run;
    if (!is_0_run) {
        if(data->running->recv_fd != -1)
            close(data->running->recv_fd);
        if(data->running->send_fd != -1)
            close(data->running->send_fd);

        ret = waitpid(data->running->pid, &status, WNOHANG);
        if (ret == -1 && data->running->pid != 0) {
            kill(data->running->pid, SIGKILL);
            waitpid(data->running->pid, &status, 0);
        }
    }
    free(data->running);
    data->running = NULL;
    if (is_0_run)
        return 1;
    return iso_stream_close(data->orig);
}


static
int extf_stream_read(IsoStream *stream, void *buf, size_t desired)
{
    int ret, blocking = 0;
    ExternalFilterStreamData *data;
    ExternalFilterRuntime *running;
    size_t fill = 0;

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = stream->data;
    running= data->running;
    if (running == NULL) {
        return ISO_FILE_NOT_OPENED;
    }
    if (running->out_eof || running->is_0_run) {
        return 0;
    }

    while (1) {
        if (running->in_eof) {

            /* >>> ??? should one replace non-blocking read() by select () ? */

            /* Make filter outlet blocking */
            ret = fcntl(running->recv_fd, F_GETFL);
            if (ret != -1) {
                ret &= ~O_NONBLOCK;
                fcntl(running->recv_fd, F_SETFL, ret);
            }
            blocking = 1;
        }

        /* Try to read desired amount from filter */;
        while (1) {

            /* >>> ??? should one replace non-blocking read() by select () ? */

            ret = read(running->recv_fd, ((char *) buf) + fill,
                       desired - fill);
            if (ret == -1) {
                if (errno == EAGAIN)
        break;
                return ISO_FILE_READ_ERROR;
            }
            fill += ret;
            running->out_counter+= ret;
            if (ret == 0) {
                running->out_eof = 1;
            }
            if (ret == 0 || fill >= desired) {
                return fill;
            }
        }

        if (running->in_eof) {

            /* >>> ??? should one replace non-blocking read() by select () ? */

            usleep(1000); /* just in case it is still non-blocking */
    continue;
        }
        if (running->pipebuf_fill) {
            ret = running->pipebuf_fill;
            running->pipebuf_fill = 0;
        } else {
            ret = iso_stream_read(data->orig, running->pipebuf,
                                  sizeof(running->pipebuf));
        }
        if (ret < 0) {
            running->in_eof = 1;
            return ret;
        }
        if (ret == 0) {
            running->in_eof = 1;
            close(running->send_fd); /* Tell the filter: it is over */
            running->send_fd = -1;
        } else {
            running->in_counter += ret;
            running->pipebuf_fill = ret;
            ret = write(running->send_fd, running->pipebuf,
                        running->pipebuf_fill);
            if (ret == -1) {
                if (errno == EAGAIN) {

                    /* >>> ??? should one replace non-blocking read()
                               by select () ? */

                    usleep(1000); /* go lazy because the filter is slow */
    continue;
                }

                /* From the view of the caller it _is_ a read error */
                running->in_eof = 1;
                return ISO_FILE_READ_ERROR;
            }
            running->pipebuf_fill = 0;
        }
    }
    return ISO_FILE_READ_ERROR; /* should never be hit */
}


static
off_t extf_stream_get_size(IsoStream *stream)
{
    int ret, ret_close;
    off_t count = 0;
    ExternalFilterStreamData *data;
    char buf[64 * 1024];
    size_t bufsize = 64 * 1024;

    if (stream == NULL) {
        return ISO_NULL_POINTER;
    }
    data = stream->data;

    if (data->size >= 0) {
        return data->size;
    }

    /* Run filter command and count output bytes */
    ret = extf_stream_open_flag(stream, 1);
    if (ret < 0) {
        return ret;
    }
    while (1) {
        ret = extf_stream_read(stream, buf, bufsize);
        if (ret <= 0)
            break;
        count += ret;
    }
    ret_close = extf_stream_close(stream);
    if (ret < 0)
        return ret;
    if (ret_close < 0)
        return ret_close;

    data->size = count;
    return count;
}


static
int extf_stream_is_repeatable(IsoStream *stream)
{
    /* Only repeatable streams are accepted as orig */
    return 1;
}


static
void extf_stream_get_id(IsoStream *stream, unsigned int *fs_id, 
                        dev_t *dev_id, ino_t *ino_id)
{
    ExternalFilterStreamData *data;

    data = stream->data;
    *fs_id = ISO_FILTER_FS_ID;
    *dev_id = ISO_FILTER_EXTERNAL_DEV_ID;
    *ino_id = data->id;
}


static
void extf_stream_free(IsoStream *stream)
{
    ExternalFilterStreamData *data;

    if (stream == NULL) {
        return;
    }
    data = stream->data;
    iso_stream_unref(data->orig);
    if (data->cmd->refcount > 0)
        data->cmd->refcount--;
    free(data);
}


static
int extf_update_size(IsoStream *stream)
{
    /* By principle size is determined only once */
    return 1;
}


IsoStreamIface extf_stream_class = {
    1,
    "extf",
    extf_stream_open,
    extf_stream_close,
    extf_stream_get_size,
    extf_stream_read,
    extf_stream_is_repeatable,
    extf_stream_get_id,
    extf_stream_free,
    extf_update_size
};


static
void extf_filter_free(FilterContext *filter)
{
    /* no data are allocated */;
}


/* To be called by iso_file_add_filter().
 * The FilterContext input parameter is not furtherly needed for the 
 * emerging IsoStream.
 */
static
int extf_filter_get_filter(FilterContext *filter, IsoStream *original, 
                  IsoStream **filtered)
{
    IsoStream *str;
    ExternalFilterStreamData *data;
    IsoExternalFilterCommand *cmd;

    if (filter == NULL || original == NULL || filtered == NULL) {
        return ISO_NULL_POINTER;
    }
    cmd = (IsoExternalFilterCommand *) filter->data;
    if (cmd->refcount + 1 <= 0) {
        return ISO_EXTF_TOO_OFTEN;
    }

    str = malloc(sizeof(IsoStream));
    if (str == NULL) {
        return ISO_OUT_OF_MEM;
    }
    data = malloc(sizeof(ExternalFilterStreamData));
    if (str == NULL) {
        free(str);
        return ISO_OUT_OF_MEM;
    }


    /* These data items are not owned by this filter object */
    data->id = ++extf_ino_id;
    data->orig = original;
    data->cmd = cmd;
    data->size = -1;
    data->running = NULL;

    /* get reference to the source */
    iso_stream_ref(data->orig);

    str->refcount = 1;
    str->data = data;
    str->class = &extf_stream_class;

    *filtered = str;

    cmd->refcount++;
    return ISO_SUCCESS;
}


/* Produce a parameter object suitable for iso_file_add_filter().
 * It may be disposed by free() after all those calls are made.
 *
 * This is an internal call of libisofs to be used by an API call that
 * attaches an IsoExternalFilterCommand to one or more IsoFile objects.
 * See libisofs.h for IsoExternalFilterCommand.
 */
static
int extf_create_context(IsoExternalFilterCommand *cmd,
                        FilterContext **filter, int flag)
{
    FilterContext *f;
    
    *filter = f = calloc(1, sizeof(FilterContext));
    if (f == NULL) {
        return ISO_OUT_OF_MEM;
    }
    f->refcount = 1;
    f->version = 0;
    f->data = cmd;
    f->free = extf_filter_free;
    f->get_filter = extf_filter_get_filter;
    return ISO_SUCCESS;
}


/*
 * A function which adds a filter to an IsoFile shall create a temporary
 * FilterContext by iso_extf_create_context(), use it in one or more calls
 * of filter.c:iso_file_add_filter() and finally dispose it by free().
 */

int iso_file_add_external_filter(IsoFile *file, IsoExternalFilterCommand *cmd,
                                 int flag)
{
    int ret;
    FilterContext *f = NULL;
    IsoStream *stream;

    ret = extf_create_context(cmd, &f, 0);
    if (ret < 0) {
        return ret;
    }
    ret = iso_file_add_filter(file, f, 0);
    free(f);
    if (ret < 0) {
        return ret;
    }
    /* Run a full filter process getsize so that the size is cached */
    stream = iso_file_get_stream(file);
    ret = iso_stream_get_size(stream);
    if (ret < 0) {
        return ret;
    }
    return ISO_SUCCESS;
}

