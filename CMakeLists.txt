cmake_minimum_required(VERSION 3.26)
project(libisofs VERSION 1.5.4)

add_library(${PROJECT_NAME} SHARED
	libisofs/builder.h
	libisofs/builder.c
	libisofs/node.h
	libisofs/node.c
	libisofs/tree.h
	libisofs/tree.c
	libisofs/find.c
	libisofs/image.h
	libisofs/image.c
	libisofs/fsource.h
	libisofs/fsource.c
	libisofs/fs_local.c
	libisofs/fs_image.c
	libisofs/messages.h
	libisofs/messages.c
	libisofs/libiso_msgs.h
	libisofs/libiso_msgs.c
	libisofs/stream.h
	libisofs/stream.c
	libisofs/filter.h
	libisofs/filter.c
	libisofs/filters/external.c
	libisofs/filters/zisofs.c
	libisofs/filters/gzip.c
	libisofs/util.h
	libisofs/util.c
	libisofs/util_rbtree.c
	libisofs/util_htable.c
	libisofs/filesrc.h
	libisofs/filesrc.c
	libisofs/ecma119.h
	libisofs/ecma119.c
	libisofs/ecma119_tree.h
	libisofs/ecma119_tree.c
	libisofs/writer.h
	libisofs/buffer.h
	libisofs/buffer.c
	libisofs/rockridge.h
	libisofs/rockridge.c
	libisofs/rockridge_read.c
	libisofs/joliet.h
	libisofs/joliet.c
	libisofs/hfsplus.h
	libisofs/hfsplus.c
	libisofs/hfsplus_decompose.c
	libisofs/hfsplus_classes.c
	libisofs/hfsplus_case.c
	libisofs/eltorito.h
	libisofs/eltorito.c
	libisofs/system_area.h
	libisofs/system_area.c
	libisofs/make_isohybrid_mbr.c
	libisofs/iso1999.h
	libisofs/iso1999.c
	libisofs/data_source.c
	libisofs/aaip_0_2.h
	libisofs/aaip_0_2.c
	libisofs/md5.h
	libisofs/md5.c
)

#libtool: compile:  gcc -DPACKAGE_NAME=\"libisofs\" -DPACKAGE_TARNAME=\"libisofs\" -DPACKAGE_VERSION=\"1.5.4\" "-DPACKAGE_STRING=\"libisofs 1.5.4\"" -DPACKAGE_BUGREPORT=\"http://libburnia-project.org\" -DPACKAGE_URL=\"\" -DPACKAGE=\"libisofs\" -DVERSION=\"1.5.4\"
# -DHAVE_STDIO_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_STRINGS_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_UNISTD_H=1 -DSTDC_HEADERS=1 -DHAVE_FSEEKO=1 -DICONV_CONST= -DHAVE_DLFCN_H=1 -DLT_OBJDIR=\".libs/\" -D_GNU_SOURCE=1 -DHAVE_TM_GMTOFF=1 -DHAVE_TIMEGM=1 -DLibburnia_timezonE=timezone -DHAVE_EACCESS=1 -DHAVE_LIBZ=1 -I. -DLibisofs_with_aaip_xattR -DLibisofs_with_sys_xattR -DLibisofs_with_zliB -DDEBUG -g -pedantic -Wall -Wextra -Wno-unused-parameter -Wno-char-subscripts -g -O2 -MT libisofs/libisofs_la-system_area.lo -MD -MP -MF libisofs/.deps/libisofs_la-system_area.Tpo -c libisofs/system_area.c  -fPIC -DPIC -o libisofs/.libs/libisofs_la-system_area.o

target_compile_definitions(${PROJECT_NAME} PRIVATE -DPACKAGE="libisofs" )
target_compile_definitions(${PROJECT_NAME} PRIVATE -DVERSION="${CMAKE_PROJECT_VERSION}" )
target_compile_definitions(${PROJECT_NAME} PRIVATE -DHAVE_INTTYPES_H=1 )
target_compile_definitions(${PROJECT_NAME} PRIVATE -DHAVE_TIMEGM=1 )

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/libisofs)
target_link_directories(${PROJECT_NAME} PUBLIC ${PROJECT_BINARY_DIR})