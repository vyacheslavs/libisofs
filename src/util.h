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

extern inline int div_up(unsigned int n, unsigned int div)
{
    return (n + div - 1) / div;
}

extern inline int round_up(unsigned int n, unsigned int mul)
{
    return div_up(n, mul) * mul;
}

int int_pow(int base, int power);

/**
 * Convert the charset encoding of a given string.
 * 
 * @param input
 *      Input string
 * @param icharset
 *      Input charset. Must be supported by iconv
 * @param ocharset
 *      Output charset. Must be supported by iconv
 * @param output
 *      Location where the pointer to the ouput string will be stored
 * @return
 *      1 on success, < 0 on error
 */
int strconv(const char *input, const char *icharset, const char *ocharset,
            char **output);

/**
 * Convert a given string from any input charset to ASCII
 * 
 * @param icharset
 *      Input charset. Must be supported by iconv
 * @param input
 *      Input string
 * @param output
 *      Location where the pointer to the ouput string will be stored
 * @return
 *      1 on success, < 0 on error
 */
int str2ascii(const char *icharset, const char *input, char **output);

/**
 * Convert a given string from any input charset to UCS-2BE charset,
 * used for Joliet file identifiers.
 * 
 * @param icharset
 *      Input charset. Must be supported by iconv
 * @param input
 *      Input string
 * @param output
 *      Location where the pointer to the ouput string will be stored
 * @return
 *      1 on success, < 0 on error
 */
int str2ucs(const char *icharset, const char *input, uint16_t **output);

/**
 * Create a level 1 directory identifier.
 * 
 * @param src
 *      The identifier, in ASCII encoding.
 */
char *iso_1_dirid(const char *src);

/**
 * Create a level 2 directory identifier.
 * 
 * @param src
 *      The identifier, in ASCII encoding.
 */
char *iso_2_dirid(const char *src);

/**
 * Create a level 1 file identifier that consists of a name, in 8.3 
 * format.
 * Note that version number is not added to the file name
 * 
 * @param src
 *      The identifier, in ASCII encoding.
 */
char *iso_1_fileid(const char *src);

/**
 * Create a level 2 file identifier.
 * Note that version number is not added to the file name
 * 
 * @param src
 *      The identifier, in ASCII encoding.
 */
char *iso_2_fileid(const char *src);

/**
 * Create a Joliet file or directory identifier that consists of name and
 * extension. The combined name and extension length will not exceed 128 bytes, 
 * and the name and extension will be separated (.). All characters consist of 
 * 2 bytes and the resulting string is NULL-terminated by a 2-byte NULL. 
 * 
 * Note that version number and (;1) is not appended.
 *
 * @return 
 *        NULL if the original name and extension both are of length 0.
 */
uint16_t *iso_j_id(const uint16_t *src);

/**
 * Like strlen, but for Joliet strings.
 */
size_t ucslen(const uint16_t *str);

/**
 * Like strrchr, but for Joliet strings.
 */
uint16_t *ucsrchr(const uint16_t *str, uint16_t c);

/**
 * Like strdup, but for Joliet strings.
 */
uint16_t *ucsdup(const uint16_t *str);

/**
 * Like strcmp, but for Joliet strings.
 */
int ucscmp(const uint16_t *s1, const uint16_t *s2);

/**
 * Like strncpy, but for Joliet strings.
 * @param n
 *      Maximum number of characters to copy (2 bytes per char).
 */
uint16_t *ucsncpy(uint16_t *dest, const uint16_t *src, size_t n);

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

uint32_t iso_read_lsb(const uint8_t *buf, int bytes);
uint32_t iso_read_msb(const uint8_t *buf, int bytes);

/**
 * if error != NULL it will be set to 1 if LSB and MSB integers don't match.
 */
uint32_t iso_read_bb(const uint8_t *buf, int bytes, int *error);

/** Records the date/time into a 7 byte buffer (ECMA-119, 9.1.5) */
void iso_datetime_7(uint8_t *buf, time_t t);

/** Records the date/time into a 17 byte buffer (ECMA-119, 8.4.26.1) */
void iso_datetime_17(uint8_t *buf, time_t t);

time_t iso_datetime_read_7(const uint8_t *buf);
time_t iso_datetime_read_17(const uint8_t *buf);

/**
 * Check whether the caller process has read access to the given local file.
 * 
 * @return 
 *     1 on success (i.e, the process has read access), < 0 on error 
 *     (including ISO_FILE_ACCESS_DENIED on access denied to the specified file
 *     or any directory on the path).
 */
int iso_eaccess(const char *path);

/**
 * Copy up to \p len chars from \p buf and return this newly allocated
 * string. The new string is null-terminated.
 */
char *strcopy(const char *buf, size_t len);

typedef struct iso_rbtree IsoRBTree;

/**
 * Create a new binary tree. libisofs binary trees allow you to add any data
 * passing it as a pointer. You must provide a function suitable for compare
 * two elements.
 *
 * @param compare
 *     A function to compare two elements. It takes a pointer to both elements
 *     and return 0, -1 or 1 if the first element is equal, less or greater 
 *     than the second one.
 * @param tree
 *     Location where the tree structure will be stored.
 */
int iso_rbtree_new(int (*compare)(const void*, const void*), IsoRBTree **tree);

/**
 * Destroy a given tree.
 * 
 * Note that only the structure itself is deleted. To delete the elements, you
 * should provide a valid free_data function. It will be called for each 
 * element of the tree, so you can use it to free any related data.
 */
void iso_rbtree_destroy(IsoRBTree *tree, void (*free_data)(void *));

/**
 * Inserts a given element in a Red-Black tree.
 *
 * @param tree
 *     the tree where to insert
 * @param data
 *     element to be inserted on the tree. It can't be NULL
 * @param item
 *     if not NULL, it will point to a location where the tree element ptr 
 *     will be stored. If data was inserted, *item == data. If data was
 *     already on the tree, *item points to the previously inserted object
 *     that is equal to data.
 * @return
 *     1 success, 0 element already inserted, < 0 error
 */
int iso_rbtree_insert(IsoRBTree *tree, void *data, void **item);

/**
 * Get the number of elements in a given tree.
 */
size_t iso_rbtree_get_size(IsoRBTree *tree);

/**
 * Get an array view of the elements of the tree.
 * 
 * @param include_item
 *    Function to select which elements to include in the array. It that takes
 *    a pointer to an element and returns 1 if the element should be included,
 *    0 if not. If you want to add all elements to the array, you can pass a
 *    NULL pointer.
 * @param size
 *    If not null, will be filled with the number of elements in the array,
 *    without counting the final NULL item.
 * @return
 *    A sorted array with the contents of the tree, or NULL if there is not
 *    enought memory to allocate the array. You should free(3) the array when
 *    no more needed. Note that the array is NULL-terminated, and thus it
 *    has size + 1 length.
 */
void **iso_rbtree_to_array(IsoRBTree *tree, int (*include_item)(void *), 
                           size_t *size);

#endif /*LIBISO_UTIL_H_*/
