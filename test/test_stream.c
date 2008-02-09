/*
 * Unit test for util.h
 * 
 * This test utiliy functions
 */
#include "test.h"
#include "stream.h"

#include <stdlib.h>

static
void test_mem_new()
{
    int ret;
    IsoStream *stream;
    unsigned char *buf;
    
    buf = malloc(3000);
    ret = iso_memory_stream_new(buf, 3000, &stream);
    CU_ASSERT_EQUAL(ret, 1);
    iso_stream_unref(stream);
    
    ret = iso_memory_stream_new(NULL, 3000, &stream);
    CU_ASSERT_EQUAL(ret, ISO_NULL_POINTER);
    
    ret = iso_memory_stream_new(buf, 3000, NULL);
    CU_ASSERT_EQUAL(ret, ISO_NULL_POINTER);
}

static
void test_mem_open()
{
    int ret;
    IsoStream *stream;
    unsigned char *buf;
    
    buf = malloc(3000);
    ret = iso_memory_stream_new(buf, 3000, &stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    ret = iso_stream_open(stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    /* try to open an already opened stream */
    ret = iso_stream_open(stream);
    CU_ASSERT_EQUAL(ret, ISO_FILE_ALREADY_OPENNED);

    ret = iso_stream_close(stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    ret = iso_stream_close(stream);
    CU_ASSERT_EQUAL(ret, ISO_FILE_NOT_OPENNED);
    
    iso_stream_unref(stream);
}

static
void test_mem_read()
{
    int ret;
    IsoStream *stream;
    unsigned char *buf;
    unsigned char rbuf[3000];
    
    buf = malloc(3000);
    memset(buf, 2, 200);
    memset(buf + 200, 3, 300);
    memset(buf + 500, 5, 500);
    memset(buf + 1000, 10, 1000);
    memset(buf + 2000, 56, 48);
    memset(buf + 2048, 137, 22);
    memset(buf + 2070, 13, 130);
    memset(buf + 2200, 88, 800);
    
    ret = iso_memory_stream_new(buf, 3000, &stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    /* test 1: read full buf */
    ret = iso_stream_open(stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    ret = iso_stream_read(stream, rbuf, 3000);
    CU_ASSERT_EQUAL(ret, 3000);
    CU_ASSERT_NSTRING_EQUAL(rbuf, buf, 3000);
    
    /* read again is EOF */
    ret = iso_stream_read(stream, rbuf, 20);
    CU_ASSERT_EQUAL(ret, 0);

    ret = iso_stream_close(stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    /* test 2: read more than available bytes */
    ret = iso_stream_open(stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    ret = iso_stream_read(stream, rbuf, 3050);
    CU_ASSERT_EQUAL(ret, 3000);
    CU_ASSERT_NSTRING_EQUAL(rbuf, buf, 3000);
    
    /* read again is EOF */
    ret = iso_stream_read(stream, rbuf, 20);
    CU_ASSERT_EQUAL(ret, 0);

    ret = iso_stream_close(stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    /* test 3: read in block size */
    ret = iso_stream_open(stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    ret = iso_stream_read(stream, rbuf, 2048);
    CU_ASSERT_EQUAL(ret, 2048);
    CU_ASSERT_NSTRING_EQUAL(rbuf, buf, 2048);
    
    ret = iso_stream_read(stream, rbuf, 2048);
    CU_ASSERT_EQUAL(ret, 3000 - 2048);
    CU_ASSERT_NSTRING_EQUAL(rbuf, buf + 2048, 3000 - 2048);
    
    ret = iso_stream_read(stream, rbuf, 20);
    CU_ASSERT_EQUAL(ret, 0);

    ret = iso_stream_close(stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    iso_stream_unref(stream);
}

static
void test_mem_size()
{
    int ret;
    off_t size;
    IsoStream *stream;
    unsigned char *buf;
    
    buf = malloc(3000);
    ret = iso_memory_stream_new(buf, 3000, &stream);
    CU_ASSERT_EQUAL(ret, 1);
    
    size = iso_stream_get_size(stream);
    CU_ASSERT_EQUAL(size, 3000);
    
    iso_stream_unref(stream);
}

void add_stream_suite()
{
    CU_pSuite pSuite = CU_add_suite("IsoStreamSuite", NULL, NULL);

    CU_add_test(pSuite, "iso_memory_stream_new()", test_mem_new);
    CU_add_test(pSuite, "MemoryStream->open()", test_mem_open);
    CU_add_test(pSuite, "MemoryStream->read()", test_mem_read);
    CU_add_test(pSuite, "MemoryStream->get_size()", test_mem_size);
}
