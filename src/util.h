/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_UTIL_H_
#define LIBISO_UTIL_H_

#include <stdint.h>
#include <time.h>

#ifndef MAX
#   define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#   define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

extern inline int div_up(int n, int div);

extern inline int round_up(int n, int mul);

int int_pow(int base, int power);

/**
 * Convert a given string from any input charset to ASCII
 * 
 * @param icharset
 *      Input charset. Must be supported by iconv
 * @param input
 *      Input string
 * @param ouput
 *      Location where the pointer to the ouput string will be stored
 * @return
 *      1 on success, < 0 on error
 */
int str2ascii(const char *icharset, const char *input, char **output);

/**
 * Ensure a given string is a valid ISO directory identifier, modifying
 * it if needed.
 * 
 * @param src
 *      The identifier, in ASCII encoding. It may be modified by the function.
 * @param maxlen
 *      Maximum length supported by current iso level.
 */
void iso_dirid(char *src, int maxlen);

/**
 * Ensure a given string is a valid ISO level 1 file identifier, in 8.3 
 * format, modifying it if needed.
 * Note that version number is not added to the file name
 * 
 * @param src
 *      The identifier, in ASCII encoding. It may be modified by the function.
 */
void iso_1_fileid(char *src);

/**
 * Ensure a given string is a valid ISO level 2 file identifier, modifying it
 * if needed.
 * Note that version number is not added to the file name
 * 
 * @param src
 *      The identifier, in ASCII encoding. It may be modified by the function.
 */
void iso_2_fileid(char *src);

/**
 * Convert a given input string to d-chars.
 * @return
 *      1 on succes, < 0 error, 0 if input was null (output is set to null)
 */
int str2d_char(const char *icharset, const char *input, char **output);
int str2a_char(const char *icharset, const char *input, char **output);

void iso_lsb(uint8_t *buf, uint32_t num, int bytes);
void iso_msb(uint8_t *buf, uint32_t num, int bytes);
void iso_bb(uint8_t *buf, uint32_t num, int bytes);

/** Records the date/time into a 7 byte buffer (ECMA-119, 9.1.5) */
void iso_datetime_7(uint8_t *buf, time_t t);

/** Records the date/time into a 17 byte buffer (ECMA-119, 8.4.26.1) */
void iso_datetime_17(uint8_t *buf, time_t t);

#endif /*LIBISO_UTIL_H_*/
