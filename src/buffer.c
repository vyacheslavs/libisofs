/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

/*
 * Synchronized ring buffer, works with a writer thread and a read thread.
 * 
 * TODO things to optimize
 *  - write/read at the end of buffer requires a second mutex_lock, even if
 *    there's enought space/data at the beginning
 *  - pre-buffer for writes < BLOCK_SIZE
 * 
 */

#include "buffer.h"
#include "error.h"

#include <pthread.h>
#include <string.h>

#define BUFFER_CAPACITY     BLOCK_SIZE * BUFFER_SIZE

#ifndef MIN
#   define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

struct iso_ring_buffer
{
    uint8_t buf[BLOCK_SIZE * BUFFER_SIZE];

    /* 
     * Number of bytes available.
     */
    size_t size;

    /* position for reading and writing, offset from buf */
    size_t rpos;
    size_t wpos;

    /* flags to report if read or writer threads ends execution */
    unsigned int rend :1;
    unsigned int wend :1;

    /* just for statistical purposes */
    unsigned int times_full;
    unsigned int times_empty;

    pthread_mutex_t mutex;
    pthread_cond_t empty;
    pthread_cond_t full;
};

/**
 * Create a new buffer.
 * 
 * The created buffer can be freed with free(3)
 * 
 * @return
 *     1 success, < 0 error
 */
int iso_ring_buffer_new(IsoRingBuffer **rbuf)
{
    IsoRingBuffer *buffer;

    if (rbuf == NULL) {
        return ISO_NULL_POINTER;
    }

    buffer = malloc(sizeof(IsoRingBuffer));
    if (buffer == NULL) {
        return ISO_MEM_ERROR;
    }

    buffer->size = 0;
    buffer->wpos = 0;
    buffer->rpos = 0;

    buffer->times_full = 0;
    buffer->times_empty = 0;

    buffer->rend = buffer->wend = 0;

    /* init mutex and waiting queues */
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->empty, NULL);
    pthread_cond_init(&buffer->full, NULL);

    *rbuf = buffer;
    return ISO_SUCCESS;
}

void iso_ring_buffer_free(IsoRingBuffer *buf)
{
    pthread_mutex_destroy(&buf->mutex);
    pthread_cond_destroy(&buf->empty);
    pthread_cond_destroy(&buf->full);
    free(buf);
}

/**
 * Write count bytes into buffer. It blocks until all bytes where written or
 * reader close the buffer.
 * 
 * @param buf
 *      the buffer
 * @param data
 *      pointer to a memory region of at least coun bytes, from which data
 *      will be read.
 * @param
 *      Number of bytes to write
 * @return
 *      1 succes, 0 read finished, < 0 error
 */
int iso_ring_buffer_write(IsoRingBuffer *buf, uint8_t *data, size_t count)
{
    size_t len;
    int bytes_write = 0;

    if (buf == NULL || data == NULL) {
        return ISO_NULL_POINTER;
    }

    while (bytes_write < count) {

        pthread_mutex_lock(&buf->mutex);

        while (buf->size == BUFFER_CAPACITY) {

            /*
             * Note. There's only a writer, so we have no race conditions.
             * Thus, the while(buf->size == BUFFER_CAPACITY) is used here
             * only to propertly detect the reader has been cancelled
             */

            if (buf->rend) {
                /* the read procces has been finished */
                pthread_mutex_unlock(&buf->mutex);
                return 0;
            }
            buf->times_full++;
            /* wait until space available */
            pthread_cond_wait(&buf->full, &buf->mutex);
        }

        len = MIN(count - bytes_write, BUFFER_CAPACITY - buf->size);
        if (buf->wpos + len > BUFFER_CAPACITY) {
            len = BUFFER_CAPACITY - buf->wpos;
        }
        memcpy(buf->buf + buf->wpos, data + bytes_write, len);
        buf->wpos = (buf->wpos + len) % (BUFFER_CAPACITY);
        bytes_write += len;
        buf->size += len;

        /* wake up reader */
        pthread_cond_signal(&buf->empty);
        pthread_mutex_unlock(&buf->mutex);
    }
    return ISO_SUCCESS;
}

/**
 * Read count bytes from the buffer into dest. It blocks until the desired
 * bytes has been read. If the writer finishes before outputting enought
 * bytes, 0 (EOF) is returned, the number of bytes already read remains
 * unknown.
 * 
 * @return
 *      1 success, 0 EOF, < 0 error
 */
int iso_ring_buffer_read(IsoRingBuffer *buf, uint8_t *dest, size_t count)
{
    size_t len;
    int bytes_read = 0;

    if (buf == NULL || dest == NULL) {
        return ISO_NULL_POINTER;
    }

    while (bytes_read < count) {
        pthread_mutex_lock(&buf->mutex);

        while (buf->size == 0) {
            /*
             * Note. There's only a reader, so we have no race conditions.
             * Thus, the while(buf->size == 0) is used here just to ensure
             * a reader detects the EOF propertly if the writer has been
             * canceled while the reader was waiting
             */

            if (buf->wend) {
                /* the writer procces has been finished */
                pthread_mutex_unlock(&buf->mutex);
                return 0; /* EOF */
            }
            buf->times_empty++;
            /* wait until data available */
            pthread_cond_wait(&buf->empty, &buf->mutex);
        }

        len = MIN(count - bytes_read, buf->size);
        if (buf->rpos + len > BUFFER_CAPACITY) {
            len = BUFFER_CAPACITY - buf->rpos;
        }
        memcpy(dest + bytes_read, buf->buf + buf->rpos, len);
        buf->rpos = (buf->rpos + len) % (BUFFER_CAPACITY);
        bytes_read += len;
        buf->size -= len;

        /* wake up the writer */
        pthread_cond_signal(&buf->full);
        pthread_mutex_unlock(&buf->mutex);
    }
    return ISO_SUCCESS;
}

void iso_ring_buffer_writer_close(IsoRingBuffer *buf)
{
    pthread_mutex_lock(&buf->mutex);
    buf->wend = 1;

    /* ensure no reader is waiting */
    pthread_cond_signal(&buf->empty);
    pthread_mutex_unlock(&buf->mutex);
}

void iso_ring_buffer_reader_close(IsoRingBuffer *buf)
{
    pthread_mutex_lock(&buf->mutex);
    buf->rend = 1;

    /* ensure no writer is waiting */
    pthread_cond_signal(&buf->full);
    pthread_mutex_unlock(&buf->mutex);
}

/**
 * Get the times the buffer was full.
 */
unsigned int iso_ring_buffer_get_times_full(IsoRingBuffer *buf)
{
    return buf->times_full;
}

/**
 * Get the times the buffer was empty.
 */
unsigned int iso_ring_buffer_get_times_empty(IsoRingBuffer *buf)
{
    return buf->times_empty;
}
