pkgconfigdir=$(libdir)/pkgconfig
libincludedir=$(includedir)/libisofs

lib_LTLIBRARIES = src/libisofs.la

## ========================================================================= ##

# Build libraries

src_libisofs_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
src_libisofs_la_SOURCES = \
	src/node.h \
	src/node.c \
	src/image.h \
	src/fsource.h \
	src/fs_local.c \
	src/stream.h 
libinclude_HEADERS = \
	src/libisofs.h

## ========================================================================= ##

## Build test applications
noinst_PROGRAMS = \
	test/iso 

test_iso_CPPFLAGS = -Isrc
test_iso_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
test_iso_SOURCES = test/iso.c


## Build unit test
 
#check_PROGRAMS = \
#	test/test
	
#test_test_CPPFLAGS = -Ilibisofs
#test_test_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS) -lcunit
#test_test_LDFLAGS = -L.. -lm
	
#test_test_SOURCES = \
#	test/test_exclude.c 

## ========================================================================= ##

## Build documentation (You need Doxygen for this to work)
## TODO!!

## ========================================================================= ##

# Extra things
nodist_pkgconfig_DATA = \
	libisofs-5.pc

EXTRA_DIST = \
	libisofs-5.pc.in \
	version.h.in \
	README \
	AUTHORS \
	COPYRIGHT \
	COPYING \
    NEWS \
    INSTALL \
    TODO \
    ChangeLog

