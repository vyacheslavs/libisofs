pkgconfigdir=$(libdir)/pkgconfig
libincludedir=$(includedir)/libisofs

lib_LTLIBRARIES = libisofs/libisofs.la

## ========================================================================= ##

# Build libraries

libisofs_libisofs_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
libisofs_libisofs_la_SOURCES = \
	libisofs/builder.h \
	libisofs/builder.c \
	libisofs/error.h \
	libisofs/node.h \
	libisofs/node.c \
	libisofs/tree.h \
	libisofs/tree.c \
	libisofs/image.h \
	libisofs/image.c \
	libisofs/fsource.h \
	libisofs/fsource.c \
	libisofs/fs_local.c \
	libisofs/fs_image.c \
	libisofs/messages.h \
	libisofs/messages.c \
	libisofs/libiso_msgs.h \
	libisofs/libiso_msgs.c \
	libisofs/stream.h \
	libisofs/stream.c \
	libisofs/util.h \
	libisofs/util.c \
	libisofs/util_rbtree.c \
	libisofs/util_htable.c \
	libisofs/filesrc.h \
	libisofs/filesrc.c \
	libisofs/ecma119.h \
	libisofs/ecma119.c \
	libisofs/ecma119_tree.h \
	libisofs/ecma119_tree.c \
	libisofs/writer.h \
	libisofs/buffer.c \
	libisofs/rockridge.h \
	libisofs/rockridge.c \
	libisofs/rockridge_read.c \
	libisofs/joliet.h \
	libisofs/joliet.c \
	libisofs/eltorito.h \
	libisofs/eltorito.c \
	libisofs/iso1999.h \
	libisofs/iso1999.c \
	libisofs/data_source.c
libinclude_HEADERS = \
	libisofs/libisofs.h

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

demo_lsl_CPPFLAGS = -Ilibisofs
demo_lsl_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_lsl_SOURCES = demo/lsl.c

demo_cat_CPPFLAGS = -Ilibisofs
demo_cat_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_cat_SOURCES = demo/cat.c

demo_catbuffer_CPPFLAGS = -Ilibisofs
demo_catbuffer_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_catbuffer_SOURCES = demo/cat_buffer.c

demo_tree_CPPFLAGS = -Ilibisofs
demo_tree_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_tree_SOURCES = demo/tree.c

demo_ecma119tree_CPPFLAGS = -Ilibisofs
demo_ecma119tree_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_ecma119tree_SOURCES = demo/ecma119_tree.c

demo_iso_CPPFLAGS = -Ilibisofs
demo_iso_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_iso_SOURCES = demo/iso.c

demo_isoread_CPPFLAGS = -Ilibisofs
demo_isoread_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_isoread_SOURCES = demo/iso_read.c

demo_isocat_CPPFLAGS = -Ilibisofs
demo_isocat_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_isocat_SOURCES = demo/iso_cat.c

demo_isomodify_CPPFLAGS = -Ilibisofs
demo_isomodify_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_isomodify_SOURCES = demo/iso_modify.c

demo_isoms_CPPFLAGS = -Ilibisofs
demo_isoms_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS)
demo_isoms_SOURCES = demo/iso_ms.c

demo_isogrow_CPPFLAGS = -Ilibisofs -Ilibburn
demo_isogrow_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS) -lburn
demo_isogrow_SOURCES = demo/iso_grow.c


## Build unit test
 
check_PROGRAMS = \
	test/test
	
test_test_CPPFLAGS = -Ilibisofs
test_test_LDADD = $(libisofs_libisofs_la_OBJECTS) $(THREAD_LIBS) -lcunit
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

