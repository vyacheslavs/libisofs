
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

#include <sys/acl.h>
#include <attr/xattr.h>

#define Aaip_acl_attrnamE "system.posix_acl_access"


/* ------------------------------ Getters --------------------------------- */

/* Obtain the ACL of the given file in long text form.
   @param path          Path to the file
   @param text          Will hold the result. This is a managed object which
                        finally has to be freed by a call to this function
                        with bit15 of flag.
   @param flag          Bitfield for control purposes
                        bit0=  obtain default ACL rather than access ACL
                        bit4=  do not return entries which match the st_mode 
                               permissions. If no other ACL entries exist:
                               set *text = NULL and return 2
                        bit15= free text and return 1
   @return              > 0 ok
                        -1  failure of system ACL service (see errno)
*/
int aaip_get_acl_text(char *path, char **text, int flag)
{
 acl_t acl= NULL;
 struct stat stbuf;
 int ret;

 if(flag & (1 << 15)) {
   if(*text != NULL)
     acl_free(text);
   *text= NULL;
   return(1);
 }
 *text= NULL;
 acl= acl_get_file(path, (flag & 1) ? ACL_TYPE_DEFAULT : ACL_TYPE_ACCESS);
 if(acl == NULL)
   return(-1);
 *text= acl_to_text(acl, NULL);
 acl_free(acl);
 if(*text == NULL)
   return(-1);
 if(flag & 16) {
   ret= stat(path, &stbuf);
   if(ret != -1)
     aaip_cleanout_st_mode(*text, stbuf.st_mode, 0);
   if((*text)[0] == 0 || strcmp(*text, "\n") == 0) {
     acl_free(text);
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
                        bit4=  do not return st_mode permissions in ACL.
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
 size_t a_acl_len= 0, d_acl_len= 0, acl_len= 0;
 unsigned char *a_acl= NULL, *d_acl= NULL, *acl= NULL;
 char *acl_text= NULL;

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
    list_size= listxattr(path, list, 0);
    if(list_size == -1)
      {ret= -1; goto ex;}
    list= calloc(list_size, 1);
    if(list == NULL)
      {ret= -1; goto ex;}
    list_size= listxattr(path, list, list_size);
    if(list_size == -1)
      {ret= -1; goto ex;}
    for(i= 0; i < list_size; i+= strlen(list + i) + 1)
      num_names++;
 }
 if(flag & 1)
   num_names++;
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

 if(flag & 1) { /* Obtain ACL */
   /* access-ACL */
   ret= aaip_get_acl_text(path, &acl_text, flag & 16);
   if(ret <= 0)
     goto ex;
   if(ret == 2)
     {ret= 1; goto ex;} /* empty ACL / only st_mode info was found in ACL */
   ret= aaip_encode_acl(acl_text, &a_acl_len, &a_acl, flag & 2);
   if(ret <= 0)
     goto ex;
   aaip_get_acl_text("", &acl_text, 1 << 15); /* free */
   
   /* eventually default-ACL */
   ret= aaip_get_acl_text(path, &acl_text, 1);
   if(ret > 0) {
     /* encode and append to a_acl */;
     ret= aaip_encode_acl(acl_text, &d_acl_len, &d_acl, (flag & 2) | 4);
     if(ret <= 0)
       goto ex;
     acl= calloc(a_acl_len + d_acl_len + 1, 1);
     if(acl == NULL)
       {ret= -1; goto ex;}
     if(a_acl_len)
       memcpy(acl, a_acl, a_acl_len);
     if(d_acl_len)
       memcpy(acl + a_acl_len, d_acl, d_acl_len);
     acl_len= a_acl_len + d_acl_len;
   } else {
     acl= a_acl;
     a_acl= NULL;
     acl_len= a_acl_len;
   }

   /* Set as attribute with empty name */;
   (*names)[*num_attrs]= strdup("");
   if((*names)[*num_attrs] == NULL)
     {ret= -1; goto ex;}
   (*values)[*num_attrs]= (char *) acl;
   (*value_lengths)[*num_attrs]= acl_len;
   (*num_attrs)++;
 }

 ret= 1;
ex:;
 if(a_acl != NULL)
   free(a_acl);
 if(d_acl != NULL)
   free(d_acl);
 if(acl_text != NULL)
   aaip_get_acl_text("", &acl_text, 1 << 15); /* free */

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
*/
int aaip_set_acl_text(char *path, char *text, int flag)
{
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
                       
*/
int aaip_set_attr_list(char *path, size_t num_attrs, char **names,
                       size_t *value_lengths, char **values, int flag)
{
 int ret, has_default_acl= 0;
 size_t i, consumed, acl_text_fill, list_size= 0, acl_idx= 0;
 char *acl_text= NULL, *list= NULL;

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
   ret= setxattr(path, names[i], values[i], value_lengths[i], 0);
   if(ret == -1)
     {ret= -4; goto ex;}
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
 ret= aaip_set_acl_text(path, acl_text, 0);
 if(ret <= 0)
   {ret= -3; goto ex;}
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


