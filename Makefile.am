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
	src/tree.h \
	src/tree.c \
	src/image.h \
	src/image.c \
	src/fsource.h \
	src/fsource.c \
	src/fs_local.c \
	src/fs_image.h \
	src/fs_image.c \
	src/messages.h \
	src/messages.c \
	src/libiso_msgs.h \
	src/libiso_msgs.c \
	src/stream.h \
	src/stream.c \
	src/util.h \
	src/util.c \
	src/util_rbtree.c \
	src/filesrc.h \
	src/filesrc.c \
	src/ecma119.h \
	src/ecma119.c \
	src/ecma119_tree.h \
	src/ecma119_tree.c \
	src/writer.h \
	src/buffer.c \
	src/rockridge.h \
	src/rockridge.c \
	src/rockridge_read.c \
	src/joliet.h \
	src/joliet.c \
	src/data_source.c
libinclude_HEADERS = \
	src/libisofs.h

## ========================================================================= ##

## Build demo applications
noinst_PROGRAMS = \
	demo/lsl \
	demo/cat \
	demo/catbuffer \
	demo/tree \
	demo/ecma119tree \
	demo/iso \
	demo/isoread \
	demo/isocat \
	demo/isomodify \
	demo/isoms \
	demo/isogrow

demo_lsl_CPPFLAGS = -Isrc
demo_lsl_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_lsl_SOURCES = demo/lsl.c

demo_cat_CPPFLAGS = -Isrc
demo_cat_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_cat_SOURCES = demo/cat.c

demo_catbuffer_CPPFLAGS = -Isrc
demo_catbuffer_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_catbuffer_SOURCES = demo/cat_buffer.c

demo_tree_CPPFLAGS = -Isrc
demo_tree_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_tree_SOURCES = demo/tree.c

demo_ecma119tree_CPPFLAGS = -Isrc
demo_ecma119tree_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_ecma119tree_SOURCES = demo/ecma119_tree.c

demo_iso_CPPFLAGS = -Isrc
demo_iso_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_iso_SOURCES = demo/iso.c

demo_isoread_CPPFLAGS = -Isrc
demo_isoread_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_isoread_SOURCES = demo/iso_read.c

demo_isocat_CPPFLAGS = -Isrc
demo_isocat_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_isocat_SOURCES = demo/iso_cat.c

demo_isomodify_CPPFLAGS = -Isrc
demo_isomodify_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_isomodify_SOURCES = demo/iso_modify.c

demo_isoms_CPPFLAGS = -Isrc
demo_isoms_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_isoms_SOURCES = demo/iso_ms.c

demo_isogrow_CPPFLAGS = -Isrc -Ilibburn
demo_isogrow_LDADD = $(src_libisofs_la_OBJECTS) $(THREAD_LIBS) -lburn
demo_isogrow_SOURCES = demo/iso_grow.c


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
	test/test_tree.c \
	test/test_util.c \
	test/test_rockridge.c \
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

