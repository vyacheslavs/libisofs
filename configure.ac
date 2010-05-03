AC_INIT([libisofs], [0.6.32], [http://libburnia-project.org])
AC_PREREQ([2.50])
dnl AC_CONFIG_HEADER([config.h])	

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

LIBBURNIA_SET_FLAGS

AM_INIT_AUTOMAKE([subdir-objects])

dnl A61101 This breaks Linux build (makes 32 bit off_t)
dnl        http://sourceware.org/autobook/autobook/autobook_96.html says
dnl        one must include some config.h and this was a pitfall.
dnl        So why dig the pit at all ?
dnl AM_CONFIG_HEADER(config.h)

dnl
dnl if MAJOR or MINOR version changes, be sure to change AC_INIT above to match
dnl
dnl CURRENT and AGE describe the binary compatibility interval of a
dnl dynamic library.
dnl See also http://www.gnu.org/software/libtool/manual.html#Interfaces
dnl
dnl The name of the library will be libisofs.so.$CURRENT-$AGE.$AGE.$REV
dnl In the terminology of this file:
dnl   CURRENT = LT_CURRENT
dnl   REV     = LT_REVISION
dnl   AGE     = LT_AGE
dnl
dnl LT_CURRENT, LT_REVISION and LT_AGE get set directly now.
dnl
dnl SONAME of the emerging library is  LT_CURRENT - LT_AGE.
dnl The linker will do no finer checks. Especially no age range check for
dnl the cdrskin binary. If SONAME matches, then the couple starts.
dnl 
dnl Therefore a run time check is provided by libisofs function 
dnl iso_lib_version(). It returns the major, minor and micro revision of the 
dnl library. This means LIBISOFS_*_VERSION kept its second job which does not  
dnl comply to the usual ways of configure.ac . I.e. now *officially* this is 
dnl the source code release version as announced to the public. It has no 
dnl connection to SONAME or libtool version numbering.
dnl It rather feeds the API function iso_lib_version().
dnl
dnl If LIBISOFS_*_VERSION changes, be sure to change AC_INIT above to match.
dnl
LIBISOFS_MAJOR_VERSION=0
LIBISOFS_MINOR_VERSION=6
LIBISOFS_MICRO_VERSION=32
LIBISOFS_VERSION=$LIBISOFS_MAJOR_VERSION.$LIBISOFS_MINOR_VERSION.$LIBISOFS_MICRO_VERSION

AC_SUBST(LIBISOFS_MAJOR_VERSION)
AC_SUBST(LIBISOFS_MINOR_VERSION)
AC_SUBST(LIBISOFS_MICRO_VERSION)
AC_SUBST(LIBISOFS_VERSION)

dnl Libtool versioning
LT_RELEASE=$LIBISOFS_MAJOR_VERSION.$LIBISOFS_MINOR_VERSION
# 2010.05.03  development jump has not yet happened
# SONAME = 34 - 28 = 6 . Library name = libisofs.6.28.0
LT_CURRENT=34
LT_AGE=28
LT_REVISION=0
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
   AC_MSG_ERROR([Libisofs requires largefile support.])
fi

dnl If iconv(3) is in an extra lib, then it gets added to variable LIBS.
dnl If not, then no -liconv will be added.
LIBBURNIA_CHECK_ICONV

dnl To abort configuration if iconv() still cannot be compiled
LIBBURNIA_ASSERT_ICONV


AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
LIBTOOL="$LIBTOOL --silent"

AC_PROG_INSTALL

AC_CHECK_HEADERS()

dnl Use GNU extensions if available
AC_DEFINE(_GNU_SOURCE, 1)

dnl Check for tm_gmtoff field in struct tm
AC_CHECK_MEMBER([struct tm.tm_gmtoff],
	[AC_DEFINE(HAVE_TM_GMTOFF, 1,
		[Define this if tm structure includes a tm_gmtoff entry.])],
	,
	[#include <time.h>])

dnl Check if non standard timegm() function is available
AC_CHECK_DECL([timegm], 
	[AC_DEFINE(HAVE_TIMEGM, 1, [Define this if timegm function is available])],
	,
	[#include <time.h>])

dnl Check if non standard eaccess() function is available
AC_CHECK_DECL([eaccess], 
	[AC_DEFINE(HAVE_EACCESS, 1, [Define this if eaccess function is available])],
	,
	[#include <unistd.h>])

THREAD_LIBS=-lpthread
AC_SUBST(THREAD_LIBS)

TARGET_SHIZZLE
AC_SUBST(ARCH)
AC_SUBST(LIBBURN_ARCH_LIBS)


dnl See if the user wants aggressive optimizations of the code
AC_ARG_ENABLE(debug,
[  --enable-debug          Disable aggressive optimizations, default=yes],
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

dnl Verbose debug to make libisofs issue more debug messages
AC_ARG_ENABLE(verbose-debug,
[  --enable-verbose-debug  Enable verbose debug messages, default=no],
	AC_DEFINE(LIBISOFS_VERBOSE_DEBUG, 1))


dnl Determine target directory for libisofs-*.pc
dnl Important: Must be performed _after_ TARGET_SHIZZLE
dnl
LIBBURNIA_SET_PKGCONFIG


dnl Add compiler-specific flags

AC_ARG_ENABLE(libacl,
[  --enable-libacl         Enable use of libacl by libisofs, default=yes],
               , enable_libacl=yes)
if test "x$enable_libacl" = xyes; then
dnl Check whether there is libacl-devel and libacl-runtime.
dnl If not, erase this macro which would enable use of acl_to_text and others
    LIBACL_DEF="-DLibisofs_with_aaip_acL"
dnl The empty yes case obviously causes -lacl to be linked
    AC_CHECK_HEADER(sys/acl.h, AC_CHECK_LIB(acl, acl_to_text, , LIBACL_DEF= ), LIBACL_DEF= )
else
    LIBACL_DEF=
fi
AC_SUBST(LIBACL_DEF)


dnl ts A90123
AC_ARG_ENABLE(xattr,
[  --enable-xattr          Enable use of xattr by libisofs, default=yes],
               , enable_xattr=yes)
if test "x$enable_xattr" = xyes; then
dnl Check whether there is the header for Linux xattr. 
dnl If not, erase this macro which would enable use of listxattr and others
    XATTR_DEF="-DLibisofs_with_aaip_xattR"
    AC_CHECK_HEADER(attr/xattr.h, AC_CHECK_LIB(c, listxattr, X= , XATTR_DEF= ), XATTR_DEF= )
else
    XATTR_DEF=
fi
AC_SUBST(XATTR_DEF)


dnl ts A90409
AC_ARG_ENABLE(zlib,
[  --enable-zlib           Enable use of zlib by libisofs, default=yes],
               , enable_zlib=yes)
if test "x$enable_zlib" = xyes; then
dnl Check whether there is the header for zlib. 
dnl If not, erase this macro which would enable use of compress2() and others.
dnl Linking fails on SuSE 9.0 because zlib has compress2() but lacks  
dnl compressBound(). So compressBound is the more modern thing to test.
dnl The empty parameter after "compressBound" causes -lz.
    ZLIB_DEF="-DLibisofs_with_zliB"
    AC_CHECK_HEADER(zlib.h, AC_CHECK_LIB(z, compressBound, , ZLIB_DEF= ), ZLIB_DEF= )
else
    ZLIB_DEF=
fi
AC_SUBST(ZLIB_DEF)


AC_CONFIG_FILES([
	Makefile
	doc/doxygen.conf
	version.h
	libisofs-1.pc
	])
AC_OUTPUT
