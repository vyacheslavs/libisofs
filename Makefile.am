pkgconfigdir=$(libdir)/pkgconfig
libincludedir=$(includedir)/libisofs

lib_LTLIBRARIES = src/libisofs.la

## ========================================================================= ##

# Build libraries

src_libisofs_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
src_libisofs_la_SOURCES = \
	src/builder.h \
	src/builder.c \
	src/error.h \
	src/node.h \
	src/node.c \
	src/tree.c \
	src/image.h \
	src/image.c \
	src/fsource.h \
	src/fsource.c \
	src/fs_local.c \
	src/messages.h \
	src/messages.c \
	src/libiso_msgs.h \
	src/libiso_msgs.c \
	src/stream.h  \
	src/stream.c 
libinclude_HEADERS = \
	src/libisofs.h

## ========================================================================= ##

## Build demo applications
noinst_PROGRAMS = \
	demo/lsl \
	demo/cat 

demo_lsl_CPPFLAGS = -Isrc
demo_lsl_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_lsl_SOURCES = demo/lsl.c

demo_cat_CPPFLAGS = -Isrc
demo_cat_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_cat_SOURCES = demo/cat.c


## Build unit test
 
check_PROGRAMS = \
	test/test
	
test_test_CPPFLAGS = -Isrc
test_test_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS) -lcunit
test_test_LDFLAGS = -L.. -lm
	
test_test_SOURCES = \
	test/test.h \
	test/test.c \
	test/test_node.c \
	test/test_image.c \
	test/test_tree.c  \
	test/mocked_fsrc.h \
	test/mocked_fsrc.c

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

