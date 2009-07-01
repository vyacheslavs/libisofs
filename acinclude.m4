AC_DEFUN([TARGET_SHIZZLE],
[
  ARCH=""
  LIBBURNIA_PKGCONFDIR="$libdir"/pkgconfig

  AC_MSG_CHECKING([target operating system])

  case $target in
    *-*-linux*)
      ARCH=linux
      LIBBURN_ARCH_LIBS=
      ;;
    *-*-freebsd*)
      ARCH=freebsd
      LIBBURN_ARCH_LIBS=-lcam
      LIBBURNIA_PKGCONFDIR=$(echo "$libdir" | sed 's/\/lib$/\/libdata/')/pkgconfig
      ;;
    *)
      ARCH=
      LIBBURN_ARCH_LIBS=
#      AC_ERROR([You are attempting to compile for an unsupported platform])
      ;;
  esac

  AC_MSG_RESULT([$ARCH])
])


dnl LIBBURNIA_CHECK_ICONV is by Thomas Schmitt, libburnia project
dnl It is based on gestures from:
dnl iconv.m4 serial AM7 (gettext-0.18)
dnl Copyright (C) 2000-2002, 2007-2009 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl From Bruno Haible.
dnl
AC_DEFUN([LIBBURNIA_CHECK_ICONV],
[
  dnl Check whether it is allowed to link with -liconv
  AC_MSG_CHECKING([for separate -liconv ])
  libburnia_liconv="no"
  libburnia_save_LIBS="$LIBS"
  LIBS="$LIBS -liconv"
  AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
    [iconv_t cd = iconv_open("","");
     iconv(cd,NULL,NULL,NULL,NULL);
     iconv_close(cd);],
     [libburnia_liconv="yes"],
     [LIBS="$libburnia_save_LIBS"]
  )
  AC_MSG_RESULT([$libburnia_liconv])

  dnl Check for iconv(..., const char **inbuf, ...)
  AC_MSG_CHECKING([for const qualifier with iconv() ])
  AC_TRY_COMPILE([
#include <stdlib.h>
#include <iconv.h>
size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
], [], [libburnia_iconv_const=""], [libburnia_iconv_const="const"]
  )
  AC_DEFINE_UNQUOTED([ICONV_CONST], [$libburnia_iconv_const])
  test -z "$libburnia_iconv_const" && libburnia_iconv_const="no"
  AC_MSG_RESULT([$libburnia_iconv_const])
])

