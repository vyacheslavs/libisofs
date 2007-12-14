/*
 * Copyright (c) 2007 Vreixo Formoso
 * Copyright (c) 2007 Mario Danic
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "util.h"
#include "error.h"

#include <stdlib.h>
#include <wchar.h>
#include <iconv.h>
#include <string.h>
#include <errno.h>

int div_up(int n, int div)
{
    return (n + div - 1) / div;
}

int round_up(int n, int mul)
{
    return div_up(n, mul) * mul;
}

/**
 * Convert a str in a specified codeset to WCHAR_T. 
 * The result must be free() when no more needed
 * 
 * @return
 *      1 success, < 0 error
 */
static 
int str2wchar(const char *icharset, const char *input, wchar_t **output) 
{
    iconv_t conv;
    size_t inbytes;
    size_t outbytes;
    char *ret;
    char *src;
    wchar_t *wstr;
    size_t n;
    
    if (icharset == NULL || input == NULL || output == NULL) {
        return ISO_NULL_POINTER;
    }

    conv = iconv_open("WCHAR_T", icharset);
    if (conv == (iconv_t)-1) {
        return ISO_CHARSET_CONV_ERROR;
    }
    
    inbytes = strlen(input);
    outbytes = (inbytes + 1) * sizeof(wchar_t);

    /* we are sure that numchars <= inbytes */
    wstr = malloc(outbytes);
    if (wstr == NULL) {
        return ISO_MEM_ERROR;
    }
    ret = (char *)wstr;
    src = (char *)input;

    n = iconv(conv, &src, &inbytes, &ret, &outbytes);
    while (n == -1) {
        
        if( errno != EINVAL ) {
            /* error, should never occur */
            iconv_close(conv);
            free(wstr);
            return ISO_CHARSET_CONV_ERROR;
        }
            
        /* invalid input string charset, just ignore */
        /* printf("String %s is not encoded in %s\n", str, codeset); */
        inbytes--;

        if (!inbytes)
            break;
        n = iconv(conv, &src, &inbytes, &ret, &outbytes);
    }
    iconv_close(conv);
    
    *( (wchar_t *)ret )='\0';
    *output = wstr;
    return ISO_SUCCESS;
}

int str2ascii(const char *icharset, const char *input, char **output)
{
    int result;
    wchar_t *wsrc_;
    char *ret;
    char *ret_;
    char *src;
    iconv_t conv;
    size_t numchars;
    size_t outbytes;
    size_t inbytes;
    size_t n;
    
    if (icharset == NULL || input == NULL || output == NULL) {
        return ISO_NULL_POINTER;
    }

    /* convert the string to a wide character string. Note: outbytes
     * is in fact the number of characters in the string and doesn't
     * include the last NULL character.
     */
    result = str2wchar(icharset, input, &wsrc_);
    if (result < 0) {
        return result;
    }
    src = (char *)wsrc_;
    numchars = wcslen(wsrc_);
    
    inbytes = numchars * sizeof(wchar_t);

    ret_ = malloc(numchars + 1);
    if (ret_ == NULL) {
        return ISO_MEM_ERROR;
    }
    outbytes = numchars;
    ret = ret_;

    /* initialize iconv */
    conv = iconv_open("ASCII", "WCHAR_T");
    if (conv == (iconv_t)-1) {
        free(wsrc_);
        free(ret_);
        return ISO_CHARSET_CONV_ERROR;
    }

    n = iconv(conv, &src, &inbytes, &ret, &outbytes);
    while(n == -1) {
        /* The destination buffer is too small. Stops here. */
        if (errno == E2BIG)
            break;

        /* An incomplete multi bytes sequence was found. We 
         * can't do anything here. That's quite unlikely. */
        if (errno == EINVAL)
            break;

        /* The last possible error is an invalid multi bytes
         * sequence. Just replace the character with a "_". 
         * Probably the character doesn't exist in ascii like
         * "é, è, à, ç, ..." in French. */
        *ret++ = '_';
        outbytes--;

        if (!outbytes)
            break;

        /* There was an error with one character but some other remain
         * to be converted. That's probably a multibyte character.
         * See above comment. */
        src += sizeof(wchar_t);
        inbytes -= sizeof(wchar_t);

        if (!inbytes)
            break;

        n = iconv(conv, &src, &inbytes, &ret, &outbytes);
    }

    iconv_close(conv);

    *ret='\0';
    free(wsrc_);
    
    *output = ret_;
    return ISO_SUCCESS;
}

