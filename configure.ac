AC_INIT([libisofs], [0.5.0], [http://libburnia-project.org])
AC_PREREQ([2.50])
dnl AC_CONFIG_HEADER([config.h])	

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([subdir-objects])

dnl A61101 This breaks Linux build (makes 32 bit off_t)
dnl        http://sourceware.org/autobook/autobook/autobook_96.html says
dnl        one must include some config.h and this was a pitfall.
dnl        So why dig the pit at all ?
dnl AM_CONFIG_HEADER(config.h)

dnl Making releases:
dnl   LIBISOFS_MICRO_VERSION += 1;
dnl   LIBISOFS_INTERFACE_AGE += 1;
dnl   LIBISOFS_BINARY_AGE += 1;
dnl if any functions have been added, set LIBISOFS_INTERFACE_AGE to 0.
dnl if backwards compatibility has been broken,
dnl set LIBISOFS_BINARY_AGE and LIBISOFS_INTERFACE_AGE to 0.
dnl
dnl if MAJOR or MINOR version changes, be sure to change AC_INIT above to match
dnl
LIBISOFS_MAJOR_VERSION=0
LIBISOFS_MINOR_VERSION=5
LIBISOFS_MICRO_VERSION=0
LIBISOFS_INTERFACE_AGE=0
LIBISOFS_BINARY_AGE=0
LIBISOFS_VERSION=$LIBISOFS_MAJOR_VERSION.$LIBISOFS_MINOR_VERSION.$LIBISOFS_MICRO_VERSION

AC_SUBST(LIBISOFS_MAJOR_VERSION)
AC_SUBST(LIBISOFS_MINOR_VERSION)
AC_SUBST(LIBISOFS_MICRO_VERSION)
AC_SUBST(LIBISOFS_INTERFACE_AGE)
AC_SUBST(LIBISOFS_BINARY_AGE)
AC_SUBST(LIBISOFS_VERSION)

dnl Libtool versioning
LT_RELEASE=$LIBISOFS_MAJOR_VERSION.$LIBISOFS_MINOR_VERSION
LT_CURRENT=`expr $LIBISOFS_MICRO_VERSION - $LIBISOFS_INTERFACE_AGE`
LT_REVISION=$LIBISOFS_INTERFACE_AGE
LT_AGE=`expr $LIBISOFS_BINARY_AGE - $LIBISOFS_INTERFACE_AGE`
LT_CURRENT_MINUS_AGE=`expr $LT_CURRENT - $LT_AGE`

AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)
AC_SUBST(LT_CURRENT_MINUS_AGE)

AC_PREFIX_DEFAULT([/usr/local])
test "$prefix" = "NONE" && prefix=$ac_default_prefix

AM_MAINTAINER_MODE

AM_PROG_CC_C_O
AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN

dnl Large file support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
AC_CHECK_FUNC([fseeko])
if test ! $ac_cv_func_fseeko; then
   AC_ERROR([Libisofs requires largefile support.])
fi

AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
LIBTOOL="$LIBTOOL --silent"

AC_PROG_INSTALL

AC_CHECK_HEADERS()

AC_CHECK_MEMBER([struct tm.tm_gmtoff],
	[AC_DEFINE(HAVE_TM_GMTOFF, 1,
		[Define this if tm structure includes a tm_gmtoff entry.])],
	,
	[#include <time.h>])

THREAD_LIBS=-lpthread
AC_SUBST(THREAD_LIBS)

TARGET_SHIZZLE
AC_SUBST(ARCH)
AC_SUBST(LIBBURN_ARCH_LIBS)

dnl Add compiler-specific flags

dnl See if the user wants aggressive optimizations of the code
AC_ARG_ENABLE(debug,
[  --enable-debug          Disable aggressive optimizations [default=yes]],
              , enable_debug=yes)
if test x$enable_debug != xyes; then
    if test x$GCC = xyes; then
        CFLAGS="$CFLAGS -O3"
        CFLAGS="$CFLAGS -fexpensive-optimizations"
    fi
    CFLAGS="$CFLAGS -DNDEBUG"
else
    if test x$GCC = xyes; then
        CFLAGS="$CFLAGS -g -pedantic -Wall"
    fi
    CFLAGS="$CFLAGS -DDEBUG"
fi

AC_CONFIG_FILES([
	Makefile
	version.h
	libisofs-5.pc
	])
AC_OUTPUT
