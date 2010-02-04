AC_DEFUN([LIBBURNIA_SET_FLAGS],
[
case $target_os in
freebsd*)
        LDFLAGS="$LDFLAGS -L/usr/local/lib"
        CPPFLAGS="$CPPFLAGS -I/usr/local/include"
        ;;
esac
])


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

      # This may later be overridden by configure --enable-libdir-pkgconfig
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


dnl LIBBURNIA_SET_PKGCONFIG determines the install directory for the *.pc file.
dnl Important: Must be performed _after_ TARGET_SHIZZLE
dnl
AC_DEFUN([LIBBURNIA_SET_PKGCONFIG],
[
### for testing --enable-libdir-pkgconfig on Linux
### LIBBURNIA_PKGCONFDIR="$libdir"data/pkgconfig

if test "x$LIBBURNIA_PKGCONFDIR" = "x$libdir"/pkgconfig
then
  dummy=dummy
else
  AC_ARG_ENABLE(libdir-pkgconfig,
  [  --enable-libdir-pkgconfig  Install to $libdir/pkgconfig on any OS, default=no],
   , enable_libdir_pkgconfig="no")
  AC_MSG_CHECKING([for --enable-libdir-pkgconfig])
  if test "x$enable_libdir_pkgconfig" = xyes
  then
    LIBBURNIA_PKGCONFDIR="$libdir"/pkgconfig
  fi
  AC_MSG_RESULT([$enable_libdir_pkgconfig])
fi

libburnia_pkgconfig_override="no"
AC_ARG_ENABLE(pkgconfig-path,
[  --enable-pkgconfig-path=DIR  Absolute path of directory for libisofs-*.pc],
libburnia_pkgconfig_override="yes" , enable_pkgconfig_path="none")
AC_MSG_CHECKING([for overridden pkgconfig directory path])
if test "x$enable_pkgconfig_path" = xno
then
  libburnia_pkgconfig_override="no"
fi
if test "x$enable_pkgconfig_path" = x -o "x$enable_pkgconfig_path" = xyes
then
  libburnia_pkgconfig_override="invalid argument"
fi
if test "x$libburnia_pkgconfig_override" = xyes
then
  LIBBURNIA_PKGCONFDIR="$enable_pkgconfig_path"
  AC_MSG_RESULT([$LIBBURNIA_PKGCONFDIR])
else
  AC_MSG_RESULT([$libburnia_pkgconfig_override])
fi
AC_SUBST(LIBBURNIA_PKGCONFDIR)

dnl For debugging only
### AC_MSG_RESULT([LIBBURNIA_PKGCONFDIR = $LIBBURNIA_PKGCONFDIR])
 
])

