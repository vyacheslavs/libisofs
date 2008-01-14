/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "libisofs.h"
#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * Little test program that reads a file and outputs it to stdout, using
 * the libisofs ring buffer as intermediate memory
 */

struct th_data
{
    IsoRingBuffer *rbuf;
    char *path;
};

#define WRITE_CHUNK 2048
#define READ_CHUNK  2048

static
void *write_function(void *arg)
{
    ssize_t bytes;
    int res;
    unsigned char tmp[WRITE_CHUNK];
    struct th_data *data = (struct th_data *) arg;

    int fd = open(data->path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Writer thread error: Can't open file");
        iso_ring_buffer_writer_close(data->rbuf, 1);
        pthread_exit(NULL);
    }

    res = 1;
    while ( (bytes = read(fd, tmp, WRITE_CHUNK)) > 0) {
        res = iso_ring_buffer_write(data->rbuf, tmp, bytes);
        if (res <= 0) {
            break;
        }
        /* To test premature reader exit >>>>>>>>>>> 
         iso_ring_buffer_writer_close(data->rbuf);
         pthread_exit(NULL);
         <<<<<<<<<<<<<<<<<<<<<<<<< */
        //        if (rand() > 2000000000) {
        //            fprintf(stderr, "Writer sleeping\n");
        //            sleep(1);
        //        }
    }
    fprintf(stderr, "Writer finish: %d\n", res);

    close(fd);
    iso_ring_buffer_writer_close(data->rbuf, 0);
    pthread_exit(NULL);
}

static
void *read_function(void *arg)
{
    unsigned char tmp[READ_CHUNK];
    int res = 1;
    struct th_data *data = (struct th_data *) arg;

    while ( (res = iso_ring_buffer_read(data->rbuf, tmp, READ_CHUNK)) > 0) {
        write(1, tmp, READ_CHUNK);
        /* To test premature reader exit >>>>>>>>>>>
         iso_ring_buffer_reader_close(data->rbuf);
         pthread_exit(NULL);
         <<<<<<<<<<<<<<<<<<<<<<<<< */
        //        if (rand() > 2000000000) {
        //            fprintf(stderr, "Reader sleeping\n");
        //            sleep(1);
        //        }
    }
    fprintf(stderr, "Reader finish: %d\n", res);

    iso_ring_buffer_reader_close(data->rbuf, 0);

    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    int res;
    struct th_data data;
    pthread_t reader;
    pthread_t writer;

    if (argc != 2) {
        fprintf(stderr, "Usage: catbuffer /path/to/file\n");
        return 1;
    }

    res = iso_ring_buffer_new(1024, &data.rbuf);
    if (res < 0) {
        fprintf(stderr, "Can't create buffer\n");
        return 1;
    }
    data.path = argv[1];

    res = pthread_create(&writer, NULL, write_function, (void *) &data);
    res = pthread_create(&reader, NULL, read_function, (void *) &data);

    pthread_join(writer, NULL);
    pthread_join(reader, NULL);

    fprintf(stderr, "Buffer was %d times full and %d times empty.\n",
            iso_ring_buffer_get_times_full(data.rbuf),
            iso_ring_buffer_get_times_empty(data.rbuf));

    free(data.rbuf);
    return 0;
}
