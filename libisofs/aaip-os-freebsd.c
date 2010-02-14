
/*

 aaip-os-freebsd.c
 Arbitrary Attribute Interchange Protocol , system adapter for getting and
 setting of ACLs and xattr.

 To be included by aaip_0_2.c

 Copyright (c) 2009 Thomas Schmitt, libburnia project, GPLv2

*/

#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef Libisofs_with_aaip_acL
/* It seems ACL is fixely integrated in FreeBSD libc. There is no libacl. */
#define Libisofs_with_aaip_acL yes
#endif

#ifdef Libisofs_with_aaip_acL
#include <sys/acl.h>
#endif


/* ------------------------------ Getters --------------------------------- */

/* Obtain the ACL of the given file in long text form.
   @param path          Path to the file
   @param text          Will hold the result. This is a managed object which
                        finally has to be freed by a call to this function
                        with bit15 of flag.
   @param flag          Bitfield for control purposes
                        (bit0=  obtain default ACL rather than access ACL)
                        bit4=  set *text = NULL and return 2
                               if the ACL matches st_mode permissions.
                        bit5=  in case of symbolic link: inquire link target
                        bit15= free text and return 1
   @return              > 0 ok
                          0 ACL support not enabled at compile time
                            or filesystem does not support ACL
                         -1 failure of system ACL service (see errno)
                         -2 attempt to inquire ACL of a symbolic
                            link without bit4 or bit5
*/
int aaip_get_acl_text(char *path, char **text, int flag)
{
#ifdef Libisofs_with_aaip_acL
 acl_t acl= NULL;
#endif
 struct stat stbuf;
 int ret;

 if(flag & (1 << 15)) {
   if(*text != NULL)
#ifdef Libisofs_with_aaip_acL
     acl_free(*text);
#else
     free(*text);
#endif
   *text= NULL;
   return(1);
 }

 *text= NULL;
 if(flag & 32)
   ret= stat(path, &stbuf);
 else
   ret= lstat(path, &stbuf);
 if(ret == -1)
   return(-1);
 if((stbuf.st_mode & S_IFMT) == S_IFLNK) {
   if(flag & 16)
     return(2);
   return(-2);
 }

 /* Note: no ACL_TYPE_DEFAULT in FreeBSD  */
 if(flag & 1)
   return(0);

#ifdef Libisofs_with_aaip_acL

 acl= acl_get_file(path, ACL_TYPE_ACCESS);

 if(acl == NULL) {
   if(errno == EOPNOTSUPP) {
     /* filesystem does not support ACL */
     if(flag & 16)
       return(2);

     /* >>> ??? fake ACL from POSIX permissions ? */;

     return(0);
   }
   return(-1);
 }
 *text= acl_to_text(acl, NULL);
 acl_free(acl);

#else /* Libisofs_with_aaip_acL */

 /* ??? >>> Fake ACL */;

 return(0);

#endif /* ! Libisofs_with_aaip_acL */

 if(*text == NULL)
   return(-1);
 if(flag & 16) {
   ret = aaip_cleanout_st_mode(*text, &(stbuf.st_mode), 2);
   if(!(ret & (7 | 64)))
     (*text)[0]= 0;
   if((*text)[0] == 0 || strcmp(*text, "\n") == 0) {
#ifdef Libisofs_with_aaip_acL
     acl_free(*text);
#else
     free(*text);
#endif
     *text= NULL;
     return(2);
   }
 }
 return(1);
}


/* Obtain the Extended Attributes and/or the ACLs of the given file in a form
   that is ready for aaip_encode().

   Note: There are no Extended Attributes in FreeBSD. So only ACL will be
         obtained.

   @param path          Path to the file
   @param num_attrs     Will return the number of name-value pairs
   @param names         Will return an array of pointers to 0-terminated names
   @param value_lengths Will return an arry with the lenghts of values
   @param values        Will return an array of pointers to 8-bit values
   @param flag          Bitfield for control purposes
                        bit0=  obtain ACL (access and eventually default)
                        bit1=  use numeric ACL qualifiers rather than names
                        bit2=  do not encode attributes other than ACL
                        bit3=  -reserved-
                        bit4=  do not return trivial ACL that matches st_mode
                        bit15= free memory of names, value_lengths, values
   @return              >0  ok
                        <=0 error
*/
int aaip_get_attr_list(char *path, size_t *num_attrs, char ***names,
                       size_t **value_lengths, char ***values, int flag)
{
 int ret;
 ssize_t i, num_names;
 size_t a_acl_len= 0, acl_len= 0;
 unsigned char *a_acl= NULL, *d_acl= NULL, *acl= NULL;
 char *acl_text= NULL;

 if(flag & (1 << 15)) { /* Free memory */
   {ret= 1; goto ex;}
 }

 *num_attrs= 0;
 *names= NULL;
 *value_lengths= NULL;
 *values= NULL;

 num_names= 0;
 if(flag & 1)
   num_names++;
 if(num_names == 0)
   {ret= 1; goto ex;}
 (*names)= calloc(num_names, sizeof(char *));
 (*value_lengths)= calloc(num_names, sizeof(size_t));
 (*values)= calloc(num_names, sizeof(char *));
 if(*names == NULL || *value_lengths == NULL || *values == NULL)
   {ret= -1; goto ex;}

 for(i= 0; i < num_names; i++) {
   (*names)[i]= NULL;
   (*values)[i]= NULL;
   (*value_lengths)[i]= 0;
 }

#ifdef Libisofs_with_aaip_acL

 if(flag & 1) { /* Obtain ACL */
   /* access-ACL */
   aaip_get_acl_text(path, &acl_text, flag & (16 | 32));
   if(acl_text == NULL)
     {ret= 1; goto ex;} /* empty ACL / only st_mode info was found in ACL */
   ret= aaip_encode_acl(acl_text, (mode_t) 0, &a_acl_len, &a_acl, flag & 2);
   if(ret <= 0)
     goto ex;
   aaip_get_acl_text("", &acl_text, 1 << 15); /* free */
   
   /* Note: There are no default-ACL in FreeBSD */

   /* Set as attribute with empty name */;
   (*names)[*num_attrs]= strdup("");
   if((*names)[*num_attrs] == NULL)
     {ret= -1; goto ex;}
   (*values)[*num_attrs]= (char *) acl;
   (*value_lengths)[*num_attrs]= acl_len;
   (*num_attrs)++;
 }

#endif /* Libisofs_with_aaip_acL */

 ret= 1;
ex:;
 if(a_acl != NULL)
   free(a_acl);
 if(d_acl != NULL)
   free(d_acl);
 if(acl_text != NULL)
   aaip_get_acl_text("", &acl_text, 1 << 15); /* free */

 if(ret <= 0 || (flag & (1 << 15))) {
   if(*names != NULL) {
     for(i= 0; i < *num_attrs; i++)
       free((*names)[i]);
     free(*names);
   }
   *names= NULL;
   if(*value_lengths != NULL)
     free(*value_lengths);
   *value_lengths= NULL;
   if(*values != NULL) {
     for(i= 0; i < *num_attrs; i++)
       free((*values)[i]);
     free(*values);
   }
   if(acl != NULL)
     free(acl);
   *values= NULL;
   *num_attrs= 0;
 }
 return(ret);
}


/* ------------------------------ Setters --------------------------------- */


/* Set the ACL of the given file to a given list in long text form.
   @param path          Path to the file
   @param text          The input text (0 terminated, ACL long text form)
   @param flag          Bitfield for control purposes
                        bit0=  set default ACL rather than access ACL
   @return              > 0 ok
                        -1  failure of system ACL service (see errno)
                        -2  ACL support not enabled at compile time
*/
int aaip_set_acl_text(char *path, char *text, int flag)
{

#ifdef Libisofs_with_aaip_acL

 int ret;
 acl_t acl= NULL;
 struct stat stbuf;

 if(flag & 32)
   ret= stat(path, &stbuf);
 else
   ret= lstat(path, &stbuf);
 if(ret == -1)
   return(-1);
 if((stbuf.st_mode & S_IFMT) == S_IFLNK)
   return(-2);

 acl= acl_from_text(text);
 if(acl == NULL) {
   ret= -1; goto ex;
 }

 /* Note: no ACL_TYPE_DEFAULT in FreeBSD */
 if(flag & 1)
   {ret= 0; goto ex;}

 ret= acl_set_file(path, ACL_TYPE_ACCESS, acl);

 if(ret == -1)
   goto ex;
 ret= 1;
ex:
 if(acl != NULL)
   acl_free(acl);
 return(ret);

#else /* Libisofs_with_aaip_acL */

 return(-2);

#endif /* ! Libisofs_with_aaip_acL */

}


/* Bring the given attributes and/or ACLs into effect with the given file.

   Note: There are no Extended Attributes in FreeBSD. So only ACL get set.

   @param flag          Bitfield for control purposes
                        bit0= decode and set ACLs
                      ( bit1= first clear all existing attributes of the file )
                      ( bit2= do not set attributes other than ACLs )
   @return              1 success
                       -1 error memory allocation
                       -2 error with decoding of ACL
                       -3 error with setting ACL
                     ( -4 error with setting attribute )
                     ( -5 error with deleting attribute )
                       -6 support of xattr not enabled at compile time
                       -7 support of ACL not enabled at compile time
*/
int aaip_set_attr_list(char *path, size_t num_attrs, char **names,
                       size_t *value_lengths, char **values, int flag)
{
 int ret, has_default_acl= 0, was_xattr= 0;
 size_t i, consumed, acl_text_fill;
 char *acl_text= NULL, *list= NULL;

 for(i= 0; i < num_attrs; i++) {
   if(names[i] == NULL || values[i] == NULL)
 continue;
   if(names[i][0] == 0) { /* Decode ACLs */
     /* access ACL */
     ret= aaip_decode_acl((unsigned char *) values[i], value_lengths[i],
                          &consumed, NULL, 0, &acl_text_fill, 1);
     if(ret <= 0)
       {ret= -2; goto ex;}
     acl_text= calloc(acl_text_fill, 1);
     if(acl_text == NULL)
       {ret= -1; goto ex;}
     ret= aaip_decode_acl((unsigned char *) values[i], value_lengths[i],
                       &consumed, acl_text, acl_text_fill, &acl_text_fill, 0);
     if(ret <= 0)
       {ret= -2; goto ex;}
     has_default_acl= (ret == 2);

#ifdef Libisofs_with_aaip_acL
     ret= aaip_set_acl_text(path, acl_text, flag & 32);
     if(ret <= 0)
       {ret= -3; goto ex;}
#else
     {ret= -7; goto ex;}
#endif
                                                            /* "default" ACL */
     if(has_default_acl) {
       free(acl_text);
       acl_text= NULL;
       ret= aaip_decode_acl((unsigned char *) (values[i] + consumed),
                            value_lengths[i] - consumed, &consumed,
                            NULL, 0, &acl_text_fill, 1);
       if(ret <= 0)
         {ret= -2; goto ex;}
       acl_text= calloc(acl_text_fill, 1);
       if(acl_text == NULL)
         {ret= -1; goto ex;}
       ret= aaip_decode_acl((unsigned char *) (values[i] + consumed),
                            value_lengths[i] - consumed, &consumed,
                            acl_text, acl_text_fill, &acl_text_fill, 0);
       if(ret <= 0)
         {ret= -2; goto ex;}
       ret= aaip_set_acl_text(path, acl_text, 1 | (flag & 32));
       if(ret <= 0)
         {ret= -3; goto ex;}
     }
   } else
     was_xattr= 1;
 }
 ret= 1;
 if(was_xattr)
   ret= -6;
ex:;
 if(acl_text != NULL)
   free(acl_text);
 if(list != NULL)
   free(list);
 return(ret);
}


