pkgconfigdir=$(libdir)/pkgconfig
libincludedir=$(includedir)/libisofs

lib_LTLIBRARIES = libisofs/libisofs.la

## ========================================================================= ##

# Build libraries

libisofs_libisofs_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

# ts A90123 : Eventually enabling system adapters for ACL and EA

libisofs_libisofs_la_CFLAGS = $(LIBACL_DEF) $(XATTR_DEF)

# ts A90114 : added aaip_0_2.*

libisofs_libisofs_la_SOURCES = \
	libisofs/builder.h \
	libisofs/builder.c \
	libisofs/node.h \
	libisofs/node.c \
	libisofs/tree.h \
	libisofs/tree.c \
	libisofs/find.c \
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
	libisofs/filter.h \
	libisofs/filter.c \
	libisofs/filters/xor_encrypt.c \
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
	libisofs/buffer.h \
	libisofs/buffer.c \
	libisofs/rockridge.h \
	libisofs/rockridge.c \
	libisofs/rockridge_read.c \
	libisofs/joliet.h \
	libisofs/joliet.c \
	libisofs/eltorito.h \
	libisofs/eltorito.c \
	libisofs/system_area.h \
	libisofs/system_area.c \
	libisofs/make_isohybrid_mbr.c \
	libisofs/iso1999.h \
	libisofs/iso1999.c \
	libisofs/data_source.c \
	libisofs/aaip_0_2.h \
	libisofs/aaip_0_2.c
libisofs_libisofs_la_LIBADD= \
	$(THREAD_LIBS)
libinclude_HEADERS = \
	libisofs/libisofs.h

## ========================================================================= ##

## Build demo applications
noinst_PROGRAMS = \
	demo/lsl \
	demo/cat \
	demo/catbuffer \
	demo/tree \
	demo/find \
	demo/ecma119tree \
	demo/iso \
	demo/isoread \
	demo/isocat \
	demo/isomodify \
	demo/isoms

#	demo/isogrow

demo_lsl_CPPFLAGS = -Ilibisofs
demo_lsl_LDADD = $(libisofs_libisofs_la_OBJECTS) $(libisofs_libisofs_la_LIBADD)
demo_lsl_SOURCES = demo/lsl.c

demo_cat_CPPFLAGS = -Ilibisofs
demo_cat_LDADD = $(libisofs_libisofs_la_OBJECTS) $(libisofs_libisofs_la_LIBADD)
demo_cat_SOURCES = demo/cat.c

demo_catbuffer_CPPFLAGS = -Ilibisofs
demo_catbuffer_LDADD = $(libisofs_libisofs_la_OBJECTS) \
	$(libisofs_libisofs_la_LIBADD)
demo_catbuffer_SOURCES = demo/cat_buffer.c

demo_tree_CPPFLAGS = -Ilibisofs
demo_tree_LDADD = $(libisofs_libisofs_la_OBJECTS) \
	$(libisofs_libisofs_la_LIBADD)
demo_tree_SOURCES = demo/tree.c

demo_find_CPPFLAGS = -Ilibisofs
demo_find_LDADD = $(libisofs_libisofs_la_OBJECTS) \
	$(libisofs_libisofs_la_LIBADD)
demo_find_SOURCES = demo/find.c

demo_ecma119tree_CPPFLAGS = -Ilibisofs
demo_ecma119tree_LDADD = $(libisofs_libisofs_la_OBJECTS) \
	$(libisofs_libisofs_la_LIBADD)
demo_ecma119tree_SOURCES = demo/ecma119_tree.c

demo_iso_CPPFLAGS = -Ilibisofs
demo_iso_LDADD = $(libisofs_libisofs_la_OBJECTS) $(libisofs_libisofs_la_LIBADD)
demo_iso_SOURCES = demo/iso.c

demo_isoread_CPPFLAGS = -Ilibisofs
demo_isoread_LDADD = $(libisofs_libisofs_la_OBJECTS) \
	$(libisofs_libisofs_la_LIBADD)
demo_isoread_SOURCES = demo/iso_read.c

demo_isocat_CPPFLAGS = -Ilibisofs
demo_isocat_LDADD = $(libisofs_libisofs_la_OBJECTS) \
	$(libisofs_libisofs_la_LIBADD)
demo_isocat_SOURCES = demo/iso_cat.c

demo_isomodify_CPPFLAGS = -Ilibisofs
demo_isomodify_LDADD = $(libisofs_libisofs_la_OBJECTS) \
	$(libisofs_libisofs_la_LIBADD)
demo_isomodify_SOURCES = demo/iso_modify.c

demo_isoms_CPPFLAGS = -Ilibisofs
demo_isoms_LDADD = $(libisofs_libisofs_la_OBJECTS) \
	$(libisofs_libisofs_la_LIBADD)
demo_isoms_SOURCES = demo/iso_ms.c

# demo_isogrow_CPPFLAGS = -Ilibisofs -Ilibburn
# demo_isogrow_LDADD = $(libisofs_libisofs_la_OBJECTS) \
# 	$(libisofs_libisofs_la_LIBADD) \
# 	-lburn
# demo_isogrow_SOURCES = demo/iso_grow.c


## Build unit test
 
check_PROGRAMS = \
	test/test
	
test_test_CPPFLAGS = -Ilibisofs
test_test_LDADD = $(libisofs_libisofs_la_OBJECTS) \
	$(libisofs_libisofs_la_LIBADD) -lcunit
test_test_LDFLAGS = -L.. -lm
	
test_test_SOURCES = \
	test/test.h \
	test/test.c \
	test/test_node.c \
	test/test_image.c \
	test/test_tree.c \
	test/test_util.c \
	test/test_rockridge.c \
	test/test_stream.c \
	test/mocked_fsrc.h \
	test/mocked_fsrc.c

## ========================================================================= ##

## Build documentation (You need Doxygen for this to work)

docdir = $(DESTDIR)$(prefix)/share/doc/$(PACKAGE)-$(VERSION)

doc:	doc/html

doc/html: doc/doxygen.conf
	$(RM) -r doc/html; \
	doxygen doc/doxygen.conf;

install-data-local: 
	if [ -d doc/html ]; then \
		$(mkinstalldirs) $(docdir)/html; \
		$(INSTALL_DATA) doc/html/* $(docdir)/html; \
	fi

uninstall-local:
	rm -rf $(docdir)

## ========================================================================= ##

# Extra things
nodist_pkgconfig_DATA = \
	libisofs-1.pc

# ts A80114 : added aaip-os*

EXTRA_DIST = \
	libisofs-1.pc.in \
	version.h.in \
	doc/doxygen.conf.in \
	doc/Tutorial \
	README \
	AUTHORS \
	COPYRIGHT \
	COPYING \
	NEWS \
	INSTALL \
	TODO \
	ChangeLog \
	Roadmap \
	doc/susp_aaip_0_2.txt \
	libisofs/aaip-os-dummy.c \
	libisofs/aaip-os-linux.c \
	libisofs/aaip-os-freebsd.c

