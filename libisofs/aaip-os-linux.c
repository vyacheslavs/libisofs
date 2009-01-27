
/*

 aaip-os-linux.c
 Arbitrary Attribute Interchange Protocol , system adapter for getting and
 setting of ACLs and XFS-style Extended Attributes.

 To be included by aaip_0_2.c
*/

#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef Libisofs_with_aaip_acL
#include <sys/acl.h>
#endif

#ifdef Libisofs_with_aaip_xattR
#include <attr/xattr.h>
#endif

#define Aaip_acl_attrnamE "system.posix_acl_access"


/* ------------------------------ Getters --------------------------------- */

/* Obtain the ACL of the given file in long text form.
   @param path          Path to the file
   @param text          Will hold the result. This is a managed object which
                        finally has to be freed by a call to this function
                        with bit15 of flag.
   @param flag          Bitfield for control purposes
                        bit0=  obtain default ACL rather than access ACL
                               behave like bit4 if ACL is empty
                        bit4=  set *text = NULL and return 2
                               if the ACL matches st_mode permissions.
                        bit15= free text and return 1
   @return                1 ok
                          2 only st_mode permissions exist and bit 4 is set
                            or empty ACL and bit0 is set
                         -1 failure of system ACL service (see errno)
                         -2 ACL support not enabled at compile time
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
     acl_free(text);
#else
     free(text);
#endif
   *text= NULL;
   return(1);
 }
 *text= NULL;

#ifdef Libisofs_with_aaip_acL

 acl= acl_get_file(path, (flag & 1) ? ACL_TYPE_DEFAULT : ACL_TYPE_ACCESS);
 if(acl == NULL)
   return(-1);
 *text= acl_to_text(acl, NULL);
 acl_free(acl);

#else /* Libisofs_with_aaip_acL */

 /* ??? >>> Fake ACL */;

 return(-2);
 
#endif /* ! Libisofs_with_aaip_acL */

 if(*text == NULL)
   return(-1);
 if(flag & 16) {
   ret= stat(path, &stbuf);
   if(ret != -1) {
     ret = aaip_cleanout_st_mode(*text, &(stbuf.st_mode), 2);
     if(!(ret & (7 | 64)))
       (*text)[0]= 0;
   }
 }
 if(flag & (1 | 16)) {
   if((*text)[0] == 0 || strcmp(*text, "\n") == 0) {
#ifdef Libisofs_with_aaip_acL
     acl_free(text);
#else
     free(text);
#endif
     *text= NULL;
     return(2);
   }
 }
 return(1);
}


/* Obtain the Extended Attributes and/or the ACLs of the given file in a form
   that is ready for aaip_encode().
   @param path          Path to the file
   @param num_attrs     Will return the number of name-value pairs
   @param names         Will return an array of pointers to 0-terminated names
   @param value_lengths Will return an arry with the lenghts of values
   @param values        Will return an array of pointers to 8-bit values
   @param flag          Bitfield for control purposes
                        bit0=  obtain ACL (access and eventually default)
                        bit1=  use numeric ACL qualifiers rather than names
                        bit2=  do not obtain attributes other than ACL
                        bit3=  do not ignore eventual local ACL attribute
                               (e.g. system.posix_acl_access)
                        bit4=  do not return trivial ACL that matches st_mode
                        bit15= free memory of names, value_lengths, values
   @return              >0  ok
                        <=0 error
*/
int aaip_get_attr_list(char *path, size_t *num_attrs, char ***names,
                       size_t **value_lengths, char ***values, int flag)
{
 int ret, retry= 0;
 char *list= NULL;
 ssize_t list_size= 0, i, num_names= 0, value_ret;
 size_t acl_len= 0;
 unsigned char *acl= NULL;
 char *a_acl_text= NULL, *d_acl_text= NULL;

 if(flag & (1 << 15)) { /* Free memory */
   if(*names != NULL)
     list= (*names)[0];
   {ret= 1; goto ex;}
 }

 *num_attrs= 0;
 *names= NULL;
 *value_lengths= NULL;
 *values= NULL;

 /* Set up arrays */
 if(!(flag & 4)) { /* Get xattr names */

#ifdef Libisofs_with_aaip_xattR

    list_size= listxattr(path, list, 0);
    if(list_size == -1)
      {ret= -1; goto ex;}
    list= calloc(list_size, 1);
    if(list == NULL)
      {ret= -1; goto ex;}
    list_size= listxattr(path, list, list_size);
    if(list_size == -1)
      {ret= -1; goto ex;}

#else /* Libisofs_with_aaip_xattR */

    list= strdup("");

#endif /* ! Libisofs_with_aaip_xattR */

    for(i= 0; i < list_size; i+= strlen(list + i) + 1)
      num_names++;
 }

#ifdef Libisofs_with_aaip_acL
 if(flag & 1)
   num_names++;
#endif

 if(num_names == 0)
   {ret= 1; goto ex;}
 (*names)= calloc(num_names, sizeof(char *));
 (*value_lengths)= calloc(num_names, sizeof(size_t));
 (*values)= calloc(num_names, sizeof(char *));
 if(*names == NULL || *value_lengths == NULL || *values == NULL)
   {ret= -1; goto ex;}

 if(!(flag & 4))
   for(i= 0; i < list_size && num_names > *num_attrs;
       i+= strlen(list + i) + 1) {
     if(!(flag & 8))
       if(strcmp(list + i, Aaip_acl_attrnamE) == 0)
   continue;
     (*names)[(*num_attrs)++]= list + i;
   }
 for(i= *num_attrs; i < num_names; i++)
   (*names)[i]= NULL;
 for(i= 0; i < num_names; i++) {
   (*values)[i]= NULL;
   (*value_lengths)[i]= 0;
 }

#ifdef Libisofs_with_aaip_xattR

 if(!(flag & 4)) { /* Get xattr values */
   for(i= 0; i < *num_attrs; i++) {
     if(!(flag & 8))
       if(strcmp((*names)[i], Aaip_acl_attrnamE) == 0)
   continue;
     value_ret= getxattr(path, (*names)[i], NULL, 0);
     if(value_ret == -1)
 continue;
     (*values)[i]= calloc(value_ret + 1, 1);
     if((*values)[i] == NULL)
       {ret= -1; goto ex;}
     (*value_lengths)[i]= getxattr(path, (*names)[i], (*values)[i], value_ret);
     if(value_ret == -1) { /* there could be a race condition */
       if(retry++ > 5)
         {ret= -1; goto ex;}
       i--;
 continue;
     }
     (*value_lengths)[i]= value_ret;
     retry= 0;
   }
 }

#endif /* Libisofs_with_aaip_xattR */

#ifdef Libisofs_with_aaip_acL

 if(flag & 1) { /* Obtain ACL */

   aaip_get_acl_text(path, &a_acl_text, flag & 16);
   aaip_get_acl_text(path, &d_acl_text, 1);
   if(a_acl_text == NULL && d_acl_text == NULL)
     {ret= 1; goto ex;}
   ret= aaip_encode_both_acl(a_acl_text, d_acl_text, (mode_t) 0,
                             &acl_len, &acl, (flag & 2));
   if(ret <= 0)
     goto ex;

   /* Set as attribute with empty name */;
   (*names)[*num_attrs]= strdup("");
   if((*names)[*num_attrs] == NULL)
     {ret= -1; goto ex;}
   (*values)[*num_attrs]= (char *) acl;
   acl= NULL;
   (*value_lengths)[*num_attrs]= acl_len;
   (*num_attrs)++;
 }

#endif /* Libisofs_with_aaip_acL */

 ret= 1;
ex:;
 if(a_acl_text != NULL)
   aaip_get_acl_text("", &a_acl_text, 1 << 15); /* free */
 if(d_acl_text != NULL)
   aaip_get_acl_text("", &d_acl_text, 1 << 15); /* free */
 if(ret <= 0 || (flag & (1 << 15))) {
   if(list != NULL)
     free(list);
   if(*names != NULL)
     free(*names);
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

 acl= acl_from_text(text);
 if(acl == NULL) {
   ret= -1; goto ex;
 }
 ret= acl_set_file(path, (flag & 1) ? ACL_TYPE_DEFAULT : ACL_TYPE_ACCESS, acl);
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
   @param flag          Bitfield for control purposes
                        bit0= decode and set ACLs
                        bit1= first clear all existing attributes of the file
                        bit2= do not set attributes other than ACLs
                        bit3= do not ignore eventual ACL attribute
                              (e.g. system.posix_acl_access)
   @return              1 success
                       -1 error memory allocation
                       -2 error with decoding of ACL
                       -3 error with setting ACL
                       -4 error with setting attribute
                       -5 error with deleting attributes
                       -6 support of xattr not enabled at compile time
                       -7 support of ACL not enabled at compile time
*/
int aaip_set_attr_list(char *path, size_t num_attrs, char **names,
                       size_t *value_lengths, char **values, int flag)
{
 int ret, has_default_acl= 0;
 size_t i, consumed, acl_text_fill, list_size= 0, acl_idx= 0;
 char *acl_text= NULL, *list= NULL;

#ifdef Libisofs_with_aaip_xattR

 if(flag & 2) /* Delete all file attributes */
   list_size= listxattr(path, list, 0);
 if(list_size > 0) { /* Delete all file attributes */
  list= calloc(list_size, 1);
  if(list == NULL)
    {ret= -5; goto ex;}
  list_size= listxattr(path, list, list_size);
  if(list_size == -1)
    {ret= -5; goto ex;}
  for(i= 0; i < list_size; i+= strlen(list + i) + 1) {
     if(!(flag & 8))
       if(strcmp(list + i, Aaip_acl_attrnamE) == 0)
   continue;
    ret= removexattr(path, list + i);
    if(ret == -1)
      {ret= -5; goto ex;}
  }
  free(list); list= NULL;
 }

#endif /* Libisofs_with_aaip_xattR */

 for(i= 0; i < num_attrs; i++) {
   if(names[i] == NULL || values[i] == NULL)
 continue;
   if(names[i][0] == 0) { /* ACLs */
     if(flag & 1)
       acl_idx= i + 1;
 continue;
   }
   /* Extended Attribute */
   if((flag & 1) && !(flag & 8))
     if(strcmp(names[i], Aaip_acl_attrnamE) == 0)
 continue;

#ifdef Libisofs_with_aaip_xattR

   ret= setxattr(path, names[i], values[i], value_lengths[i], 0);
   if(ret == -1)
     {ret= -4; goto ex;}

#else

   {ret= -6; goto ex;}

#endif /* Libisofs_with_aaip_xattR */

 }

/* Decode ACLs */
 if(acl_idx == 0)
   {ret= 1; goto ex;}
 i= acl_idx - 1;
                                                             /* "access" ACL */
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
 ret= aaip_set_acl_text(path, acl_text, 0);
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
   ret= aaip_set_acl_text(path, acl_text, 1);
   if(ret <= 0)
     {ret= -3; goto ex;}
 }
 ret= 1;
ex:;
 if(acl_text != NULL)
   free(acl_text);
 if(list != NULL)
   free(list);
 return(ret);
}

