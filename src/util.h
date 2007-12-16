/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#ifndef LIBISO_UTIL_H_
#define LIBISO_UTIL_H_

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

#endif /*LIBISO_UTIL_H_*/
