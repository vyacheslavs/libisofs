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
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <locale.h>
#include <langinfo.h>

/* for eaccess, defined in unistd.h */
#define __USE_GNU
#include <unistd.h>

inline int div_up(unsigned int n, unsigned int div)
{
    return (n + div - 1) / div;
}

inline int round_up(unsigned int n, unsigned int mul)
{
    return div_up(n, mul) * mul;
}

int int_pow(int base, int power)
{
    int result = 1;
    while (--power >= 0) {
        result *= base;
    }
    return result;
}

int strconv(const char *str, const char *icharset, const char *ocharset,
            char **output)
{
    size_t inbytes;
    size_t outbytes;
    size_t n;
    iconv_t conv;
    char *out;
    char *src;
    char *ret;

    inbytes = strlen(str);
    outbytes = (inbytes + 1) * MB_LEN_MAX;
    out = alloca(outbytes);
    if (out == NULL) {
        return ISO_MEM_ERROR;
    }

    conv = iconv_open(ocharset, icharset);
    if (conv == (iconv_t)(-1)) {
        return ISO_CHARSET_CONV_ERROR;
    }
    src = (char *)str;
    ret = (char *)out;

    n = iconv(conv, &src, &inbytes, &ret, &outbytes);
    if (n == -1) {
        /* error */
        iconv_close(conv);
        return ISO_CHARSET_CONV_ERROR;
    }
    *ret = '\0';
    iconv_close(conv);

    *output = malloc(ret - out + 1);
    if (*output == NULL) {
        return ISO_MEM_ERROR;
    }
    memcpy(*output, out, ret - out + 1);
    return ISO_SUCCESS;
}

int strnconv(const char *str, const char *icharset, const char *ocharset,
             size_t len, char **output)
{
    size_t inbytes;
    size_t outbytes;
    size_t n;
    iconv_t conv;
    char *out;
    char *src;
    char *ret;

    inbytes = len;
    outbytes = (inbytes + 1) * MB_LEN_MAX;
    out = alloca(outbytes);
    if (out == NULL) {
        return ISO_MEM_ERROR;
    }

    conv = iconv_open(ocharset, icharset);
    if (conv == (iconv_t)(-1)) {
        return ISO_CHARSET_CONV_ERROR;
    }
    src = (char *)str;
    ret = (char *)out;

    n = iconv(conv, &src, &inbytes, &ret, &outbytes);
    if (n == -1) {
        /* error */
        iconv_close(conv);
        return ISO_CHARSET_CONV_ERROR;
    }
    *ret = '\0';
    iconv_close(conv);

    *output = malloc(ret - out + 1);
    if (*output == NULL) {
        return ISO_MEM_ERROR;
    }
    memcpy(*output, out, ret - out + 1);
    return ISO_SUCCESS;
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

        if (errno == E2BIG) {
            /* error, should never occur */
            iconv_close(conv);
            free(wstr);
            return ISO_CHARSET_CONV_ERROR;
        } else {
            wchar_t *wret;

            /* 
             * Invalid input string charset.
             * This can happen if input is in fact encoded in a charset 
             * different than icharset.
             * We can't do anything better than replace by "_" and continue.
             */
            /* TODO we need a way to report this */
            /* printf("String %s is not encoded in %s\n", str, codeset); */
            inbytes--;
            src++;

            wret = (wchar_t*) ret;
            *wret++ = (wchar_t) '_';
            ret = (char *) wret;
            outbytes -= sizeof(wchar_t);

            if (!inbytes)
                break;
            n = iconv(conv, &src, &inbytes, &ret, &outbytes);
        }
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
    while (n == -1) {
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

static
void set_ucsbe(uint16_t *ucs, char c)
{
    char *v = (char*)ucs;
    v[0] = (char)0;
    v[1] = c;
}

/**
 * @return
 *      -1, 0, 1 if *ucs <, == or > than c
 */
static
int cmp_ucsbe(const uint16_t *ucs, char c)
{
    char *v = (char*)ucs;
    if (v[0] != 0) {
        return 1;
    } else if (v[1] == c) {
        return 0;
    } else {
        return (uint8_t)c > (uint8_t)v[1] ? -1 : 1;
    }
}

int str2ucs(const char *icharset, const char *input, uint16_t **output)
{
    int result;
    wchar_t *wsrc_;
    char *src;
    char *ret;
    char *ret_;
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

    ret_ = malloc((numchars+1) * sizeof(uint16_t));
    if (ret_ == NULL) {
        return ISO_MEM_ERROR;
    }
    outbytes = numchars * sizeof(uint16_t);
    ret = ret_;

    /* initialize iconv */
    conv = iconv_open("UCS-2BE", "WCHAR_T");
    if (conv == (iconv_t)-1) {
        free(wsrc_);
        free(ret_);
        return ISO_CHARSET_CONV_ERROR;
    }

    n = iconv(conv, &src, &inbytes, &ret, &outbytes);
    while (n == -1) {
        /* The destination buffer is too small. Stops here. */
        if (errno == E2BIG)
            break;

        /* An incomplete multi bytes sequence was found. We 
         * can't do anything here. That's quite unlikely. */
        if (errno == EINVAL)
            break;

        /* The last possible error is an invalid multi bytes
         * sequence. Just replace the character with a "_". 
         * Probably the character doesn't exist in UCS */
        set_ucsbe((uint16_t*) ret, '_');
        ret += sizeof(uint16_t);
        outbytes -= sizeof(uint16_t);

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

    /* close the ucs string */
    set_ucsbe((uint16_t*) ret, '\0');
    free(wsrc_);

    *output = (uint16_t*)ret_;
    return ISO_SUCCESS;
}

static int valid_d_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c == '_');
}

static int valid_a_char(char c)
{
    return (c >= ' ' && c <= '"') || (c >= '%' && c <= '?') || 
           (c >= 'A' && c <= 'Z') || (c == '_');
}

static int valid_j_char(uint16_t c)
{
    return cmp_ucsbe(&c, ' ') != -1 && cmp_ucsbe(&c, '*') && cmp_ucsbe(&c, '/')
        && cmp_ucsbe(&c, ':') && cmp_ucsbe(&c, ';') && cmp_ucsbe(&c, '?') 
        && cmp_ucsbe(&c, '\\');
}

static
char *iso_dirid(const char *src, int size)
{
    size_t len, i;
    char name[32];

    len = strlen(src);
    if (len > size) {
        len = size;
    }
    for (i = 0; i < len; i++) {
        char c= toupper(src[i]);
        name[i] = valid_d_char(c) ? c : '_';
    }

    name[len] = '\0';
    return strdup(name);
}

char *iso_1_dirid(const char *src)
{
    return iso_dirid(src, 8);
}

char *iso_2_dirid(const char *src)
{
    return iso_dirid(src, 31);
}

char *iso_1_fileid(const char *src)
{
    char *dot; /* Position of the last dot in the filename, will be used 
                * to calculate lname and lext. */
    int lname, lext, pos, i;
    char dest[13]; /*  13 = 8 (name) + 1 (.) + 3 (ext) + 1 (\0) */

    if (src == NULL) {
        return NULL;
    }
    dot = strrchr(src, '.');

    lext = dot ? strlen(dot + 1) : 0;
    lname = strlen(src) - lext - (dot ? 1 : 0);

    /* If we can't build a filename, return NULL. */
    if (lname == 0 && lext == 0) {
        return NULL;
    }

    pos = 0;

    /* Convert up to 8 characters of the filename. */
    for (i = 0; i < lname && i < 8; i++) {
        char c= toupper(src[i]);

        dest[pos++] = valid_d_char(c) ? c : '_';
    }

    /* This dot is mandatory, even if there is no extension. */
    dest[pos++] = '.';

    /* Convert up to 3 characters of the extension, if any. */
    for (i = 0; i < lext && i < 3; i++) {
        char c= toupper(src[lname + 1 + i]);

        dest[pos++] = valid_d_char(c) ? c : '_';
    }

    dest[pos] = '\0';
    return strdup(dest);
}

char *iso_2_fileid(const char *src)
{
    char *dot;
    int lname, lext, lnname, lnext, pos, i;
    char dest[32]; /* 32 = 30 (name + ext) + 1 (.) + 1 (\0) */

    if (src == NULL) {
        return NULL;
    }

    dot = strrchr(src, '.');

    /* 
     * Since the maximum length can be divided freely over the name and
     * extension, we need to calculate their new lengths (lnname and
     * lnext). If the original filename is too long, we start by trimming
     * the extension, but keep a minimum extension length of 3. 
     */
    if (dot == NULL || *(dot + 1) == '\0') {
        lname = strlen(src);
        lnname = (lname > 30) ? 30 : lname;
        lext = lnext = 0;
    } else {
        lext = strlen(dot + 1);
        lname = strlen(src) - lext - 1;
        lnext = (strlen(src) > 31 && lext > 3) ? (lname < 27 ? 30 - lname : 3)
                : lext;
        lnname = (strlen(src) > 31) ? 30 - lnext : lname;
    }

    if (lnname == 0 && lnext == 0) {
        return NULL;
    }

    pos = 0;

    /* Convert up to lnname characters of the filename. */
    for (i = 0; i < lnname; i++) {
        char c= toupper(src[i]);

        dest[pos++] = valid_d_char(c) ? c : '_';
    }
    dest[pos++] = '.';

    /* Convert up to lnext characters of the extension, if any. */
    for (i = 0; i < lnext; i++) {
        char c= toupper(src[lname + 1 + i]);

        dest[pos++] = valid_d_char(c) ? c : '_';
    }
    dest[pos] = '\0';
    return strdup(dest);
}

/**
 * Create a dir name suitable for an ISO image with relaxed constraints.
 * 
 * @param size
 *     Max len for the name
 * @param relaxed
 *     0 only allow d-characters, 1 allow also lowe case chars, 
 *     2 allow all characters 
 */
char *iso_r_dirid(const char *src, int size, int relaxed)
{
    size_t len, i;
    char *dest;

    len = strlen(src);
    if (len > size) {
        len = size;
    }
    dest = malloc(len + 1);
    for (i = 0; i < len; i++) {
        char c= src[i];
        if (relaxed == 2) {
            /* all chars are allowed */
            dest[i] = c;
        } else if (valid_d_char(c)) {
            /* it is a valid char */
            dest[i] = c;
        } else {
            c= toupper(src[i]);
            if (valid_d_char(c)) {
                if (relaxed) {
                    /* lower chars are allowed */
                    dest[i] = src[i];
                } else {
                    dest[i] = c;
                }
            } else {
                dest[i] = '_';
            }
        }
    }

    dest[len] = '\0';
    return dest;
}

/**
 * Create a file name suitable for an ISO image with relaxed constraints.
 * 
 * @param len
 *     Max len for the name, without taken the "." into account.
 * @param relaxed
 *     0 only allow d-characters, 1 allow also lowe case chars, 
 *     2 allow all characters 
 * @param forcedot
 *     Whether to ensure that "." is added
 */
char *iso_r_fileid(const char *src, size_t len, int relaxed, int forcedot)
{
    char *dot;
    int lname, lext, lnname, lnext, pos, i;
    char *dest = alloca(len + 1 + 1);

    if (src == NULL) {
        return NULL;
    }

    dot = strrchr(src, '.');

    /* 
     * Since the maximum length can be divided freely over the name and
     * extension, we need to calculate their new lengths (lnname and
     * lnext). If the original filename is too long, we start by trimming
     * the extension, but keep a minimum extension length of 3. 
     */
    if (dot == NULL || *(dot + 1) == '\0') {
        lname = strlen(src);
        lnname = (lname > len) ? len : lname;
        lext = lnext = 0;
    } else {
        lext = strlen(dot + 1);
        lname = strlen(src) - lext - 1;
        lnext = (strlen(src) > len + 1 && lext > 3) ? 
                (lname < len - 3 ? len - lname : 3)
                : lext;
        lnname = (strlen(src) > len + 1) ? len - lnext : lname;
    }

    if (lnname == 0 && lnext == 0) {
        return NULL;
    }

    pos = 0;

    /* Convert up to lnname characters of the filename. */
    for (i = 0; i < lnname; i++) {
        char c= src[i];
        if (relaxed == 2) {
            /* all chars are allowed */
            dest[pos++] = c;
        } else if (valid_d_char(c)) {
            /* it is a valid char */
            dest[pos++] = c;
        } else {
            c= toupper(src[i]);
            if (valid_d_char(c)) {
                if (relaxed) {
                    /* lower chars are allowed */
                    dest[pos++] = src[i];
                } else {
                    dest[pos++] = c;
                }
            } else {
                dest[pos++] = '_';
            }
        }
    }
    if (lnext > 0 || forcedot) {
        dest[pos++] = '.';
    }

    /* Convert up to lnext characters of the extension, if any. */
    for (i = lname + 1; i < lname + 1 + lnext; i++) {
        char c= src[i];
        if (relaxed == 2) {
            /* all chars are allowed */
            dest[pos++] = c;
        } else if (valid_d_char(c)) {
            /* it is a valid char */
            dest[pos++] = c;
        } else {
            c= toupper(src[i]);
            if (valid_d_char(c)) {
                if (relaxed) {
                    /* lower chars are allowed */
                    dest[pos++] = src[i];
                } else {
                    dest[pos++] = c;
                }
            } else {
                dest[pos++] = '_';
            }
        }
    }
    dest[pos] = '\0';
    return strdup(dest);
}

uint16_t *iso_j_file_id(const uint16_t *src)
{
    uint16_t *dot;
    size_t lname, lext, lnname, lnext, pos, i;
    uint16_t dest[66]; /* 66 = 64 (name + ext) + 1 (.) + 1 (\0) */

    if (src == NULL) {
        return NULL;
    }

    dot = ucsrchr(src, '.');

    /* 
     * Since the maximum length can be divided freely over the name and
     * extension, we need to calculate their new lengths (lnname and
     * lnext). If the original filename is too long, we start by trimming
     * the extension, but keep a minimum extension length of 3. 
     */
    if (dot == NULL || cmp_ucsbe(dot + 1, '\0') == 0) {
        lname = ucslen(src);
        lnname = (lname > 64) ? 64 : lname;
        lext = lnext = 0;
    } else {
        lext = ucslen(dot + 1);
        lname = ucslen(src) - lext - 1;
        lnext = (ucslen(src) > 65 && lext > 3) ? (lname < 61 ? 64 - lname : 3)
                : lext;
        lnname = (ucslen(src) > 65) ? 64 - lnext : lname;
    }

    if (lnname == 0 && lnext == 0) {
        return NULL;
    }

    pos = 0;

    /* Convert up to lnname characters of the filename. */
    for (i = 0; i < lnname; i++) {
        uint16_t c = src[i];
        if (valid_j_char(c)) {
            dest[pos++] = c;
        } else {
            set_ucsbe(dest + pos, '_');
            pos++;
        }
    }
    set_ucsbe(dest + pos, '.');
    pos++;

    /* Convert up to lnext characters of the extension, if any. */
    for (i = 0; i < lnext; i++) {
        uint16_t c = src[lname + 1 + i];
        if (valid_j_char(c)) {
            dest[pos++] = c;
        } else {
            set_ucsbe(dest + pos, '_');
            pos++;
        }
    }
    set_ucsbe(dest + pos, '\0');
    return ucsdup(dest);
}

uint16_t *iso_j_dir_id(const uint16_t *src)
{
    size_t len, i;
    uint16_t dest[65]; /* 65 = 64 + 1 (\0) */

    if (src == NULL) {
        return NULL;
    }

    len = ucslen(src);
    if (len > 64) {
        len = 64;
    }
    for (i = 0; i < len; i++) {
        uint16_t c = src[i];
        if (valid_j_char(c)) {
            dest[i] = c;
        } else {
            set_ucsbe(dest + i, '_');
        }
    }
    set_ucsbe(dest + len, '\0');
    return ucsdup(dest);
}

size_t ucslen(const uint16_t *str)
{
    size_t i;

    for (i = 0; str[i]; i++)
        ;
    return i;
}

uint16_t *ucsrchr(const uint16_t *str, char c)
{
    size_t len = ucslen(str);

    while (len-- > 0) {
        if (cmp_ucsbe(str + len, c) == 0) {
            return (uint16_t*)(str + len);
        }
    }
    return NULL;
}

uint16_t *ucsdup(const uint16_t *str)
{
    uint16_t *ret;
    size_t len = ucslen(str);
    
    ret = malloc(2 * (len + 1));
    if (ret != NULL) {
        memcpy(ret, str, 2 * (len + 1));
    }
    return ret;
}

/**
 * Although each character is 2 bytes, we actually compare byte-by-byte
 * (thats what the spec says).
 */
int ucscmp(const uint16_t *s1, const uint16_t *s2)
{
    const char *s = (const char*)s1;
    const char *t = (const char*)s2;
    size_t len1 = ucslen(s1);
    size_t len2 = ucslen(s2);
    size_t i, len = MIN(len1, len2) * 2;

    for (i = 0; i < len; i++) {
        if (s[i] < t[i]) {
            return -1;
        } else if (s[i] > t[i]) {
            return 1;
        }
    }

    if (len1 < len2)
        return -1;
    else if (len1 > len2)
        return 1;
    return 0;
}

uint16_t *ucsncpy(uint16_t *dest, const uint16_t *src, size_t n)
{
    n = MIN(n, ucslen(src) + 1);
    memcpy(dest, src, n*2);
    return dest;
}

int str2d_char(const char *icharset, const char *input, char **output)
{
    int ret;
    char *ascii;
    size_t len, i;

    if (output == NULL) {
        return ISO_MEM_ERROR;
    }

    /** allow NULL input */
    if (input == NULL) {
        *output = NULL;
        return 0;
    }

    /* this checks for NULL parameters */
    ret = str2ascii(icharset, input, &ascii);
    if (ret < 0) {
        *output = NULL;
        return ret;
    }

    len = strlen(ascii);

    for (i = 0; i < len; ++i) {
        char c= toupper(ascii[i]);
        ascii[i] = valid_d_char(c) ? c : '_';
    }

    *output = ascii;
    return ISO_SUCCESS;
}

int str2a_char(const char *icharset, const char *input, char **output)
{
    int ret;
    char *ascii;
    size_t len, i;

    if (output == NULL) {
        return ISO_MEM_ERROR;
    }

    /** allow NULL input */
    if (input == NULL) {
        *output = NULL;
        return 0;
    }

    /* this checks for NULL parameters */
    ret = str2ascii(icharset, input, &ascii);
    if (ret < 0) {
        *output = NULL;
        return ret;
    }

    len = strlen(ascii);

    for (i = 0; i < len; ++i) {
        char c= toupper(ascii[i]);
        ascii[i] = valid_a_char(c) ? c : '_';
    }

    *output = ascii;
    return ISO_SUCCESS;
}

void iso_lsb(uint8_t *buf, uint32_t num, int bytes)
{
    int i;

    for (i = 0; i < bytes; ++i)
        buf[i] = (num >> (8 * i)) & 0xff;
}

void iso_msb(uint8_t *buf, uint32_t num, int bytes)
{
    int i;

    for (i = 0; i < bytes; ++i)
        buf[bytes - 1 - i] = (num >> (8 * i)) & 0xff;
}

void iso_bb(uint8_t *buf, uint32_t num, int bytes)
{
    iso_lsb(buf, num, bytes);
    iso_msb(buf+bytes, num, bytes);
}

uint32_t iso_read_lsb(const uint8_t *buf, int bytes)
{
    int i;
    uint32_t ret = 0;

    for (i=0; i<bytes; i++) {
        ret += ((uint32_t) buf[i]) << (i*8);
    }
    return ret;
}

uint32_t iso_read_msb(const uint8_t *buf, int bytes)
{
    int i;
    uint32_t ret = 0;

    for (i=0; i<bytes; i++) {
        ret += ((uint32_t) buf[bytes-i-1]) << (i*8);
    }
    return ret;
}

uint32_t iso_read_bb(const uint8_t *buf, int bytes, int *error)
{
    uint32_t v1 = iso_read_lsb(buf, bytes);

    if (error) {
        uint32_t v2 = iso_read_msb(buf + bytes, bytes);
        if (v1 != v2) 
            *error = 1;
    }
    return v1;
}

void iso_datetime_7(unsigned char *buf, time_t t)
{
    static int tzsetup = 0;
    int tzoffset;
    struct tm tm;

    if (!tzsetup) {
        tzset();
        tzsetup = 1;
    }

    localtime_r(&t, &tm);

    buf[0] = tm.tm_year;
    buf[1] = tm.tm_mon + 1;
    buf[2] = tm.tm_mday;
    buf[3] = tm.tm_hour;
    buf[4] = tm.tm_min;
    buf[5] = tm.tm_sec;
#ifdef HAVE_TM_GMTOFF
    tzoffset = tm.tm_gmtoff / 60 / 15;
#else
    tzoffset = timezone / 60 / 15;
#endif
    if (tzoffset > 52)
        tzoffset -= 101;
    buf[6] = tzoffset;
}

void iso_datetime_17(unsigned char *buf, time_t t)
{
    static int tzsetup = 0;
    static int tzoffset;
    struct tm tm;

    if (t == (time_t) - 1) {
        /* unspecified time */
        memset(buf, '0', 16);
        buf[16] = 0;
    } else {
        if (!tzsetup) {
            tzset();
            tzsetup = 1;
        }

        localtime_r(&t, &tm);

        sprintf((char*)&buf[0], "%04d", tm.tm_year + 1900);
        sprintf((char*)&buf[4], "%02d", tm.tm_mon + 1);
        sprintf((char*)&buf[6], "%02d", tm.tm_mday);
        sprintf((char*)&buf[8], "%02d", tm.tm_hour);
        sprintf((char*)&buf[10], "%02d", tm.tm_min);
        sprintf((char*)&buf[12], "%02d", MIN(59, tm.tm_sec));
        memcpy(&buf[14], "00", 2);
#ifdef HAVE_TM_GMTOFF
        tzoffset = tm.tm_gmtoff / 60 / 15;
#else
        tzoffset = timezone / 60 / 15;
#endif
        if (tzoffset > 52)
            tzoffset -= 101;
        buf[16] = tzoffset;
    }
}

time_t iso_datetime_read_7(const uint8_t *buf)
{
    struct tm tm;

    tm.tm_year = buf[0];
    tm.tm_mon = buf[1] - 1;
    tm.tm_mday = buf[2];
    tm.tm_hour = buf[3];
    tm.tm_min = buf[4];
    tm.tm_sec = buf[5];
    return timegm(&tm) - buf[6] * 60 * 15;
}

time_t iso_datetime_read_17(const uint8_t *buf)
{
    struct tm tm;

    sscanf((char*)&buf[0], "%4d", &tm.tm_year);
    sscanf((char*)&buf[4], "%2d", &tm.tm_mon);
    sscanf((char*)&buf[6], "%2d", &tm.tm_mday);
    sscanf((char*)&buf[8], "%2d", &tm.tm_hour);
    sscanf((char*)&buf[10], "%2d", &tm.tm_min);
    sscanf((char*)&buf[12], "%2d", &tm.tm_sec);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;

    return timegm(&tm) - buf[16] * 60 * 15;
}

/**
 * Check whether the caller process has read access to the given local file.
 * 
 * @return 
 *     1 on success (i.e, the process has read access), < 0 on error 
 *     (including ISO_FILE_ACCESS_DENIED on access denied to the specified file
 *     or any directory on the path).
 */
int iso_eaccess(const char *path)
{
    // TODO replace non-standard eaccess with our own implementation
    if (eaccess(path, R_OK) != 0) {
        int err;

        /* error, choose an appropriate return code */
        switch (errno) {
        case EACCES:
            err = ISO_FILE_ACCESS_DENIED;
            break;
        case ENOTDIR:
        case ENAMETOOLONG:
        case ELOOP:
            err = ISO_FILE_BAD_PATH;
            break;
        case ENOENT:
            err = ISO_FILE_DOESNT_EXIST;
            break;
        case EFAULT:
        case ENOMEM:
            err = ISO_MEM_ERROR;
            break;
        default:
            err = ISO_FILE_ERROR;
            break;
        }
        return err;
    }
    return ISO_SUCCESS;
}

char *strcopy(const char *buf, size_t len)
{
    char *str;
    
    str = malloc((len + 1) * sizeof(char));
    if (str == NULL) {
        return NULL;
    }
    strncpy(str, buf, len);
    str[len] = '\0';
    
    /* remove trailing spaces */
    for (len = len-1; str[len] == ' ' && len > 0; --len)
        str[len] = '\0'; 
    
    return str;
}

/**
 * Copy up to \p max characters from \p src to \p dest. If \p src has less than
 * \p max characters, we pad dest with " " characters.
 */
void strncpy_pad(char *dest, const char *src, size_t max)
{
    size_t len, i;
    
    if (src != NULL) {
        len = MIN(strlen(src), max);
    } else {
        len = 0;
    }
    
    for (i = 0; i < len; ++i)
        dest[i] = src[i];
    for (i = len; i < max; ++i) 
        dest[i] = ' ';
}

char *ucs2str(const char *buf, size_t len)
{
    size_t outbytes, inbytes;
    char *str, *src, *out;
    iconv_t conv;
    size_t n;
    
    inbytes = len;
    
    outbytes = (inbytes+1) * MB_LEN_MAX;
    
    /* ensure enought space */
    out = alloca(outbytes);

    /* convert to local charset */
    setlocale(LC_CTYPE, "");
    conv = iconv_open(nl_langinfo(CODESET), "UCS-2BE");
    if (conv == (iconv_t)(-1)) {
        return NULL;
    }
    src = (char *)buf;
    str = (char *)out;

    n = iconv(conv, &src, &inbytes, &str, &outbytes);
    if (n == -1) {
        /* error */
        iconv_close(conv);
        return NULL;
    }
    iconv_close(conv);
    *str = '\0';

    /* remove trailing spaces */
    for (len = strlen(out) - 1; out[len] == ' ' && len > 0; --len)
        out[len] = '\0';
    return strdup(out);
}
