AM_CFLAGS = @WARNINGS@

noinst_LIBRARIES = libfilesystems.a
libfilesystems_a_SOURCES = \
   error.c mountlist.c xmalloc.c \
   common.h error.h xalloc.h mountlist.h nputils.h compat_getopt.h
libfilesystems_a_LIBADD = $(LIBOBJS)

libexec_PROGRAMS = check_readonlyfs check_ifmount

check_readonlyfs_SOURCES = check_readonlyfs.c
check_readonlyfs_LDADD = libfilesystems.a

check_ifmount_SOURCES = check_ifmount.c
check_ifmount_LDADD = libfilesystems.a
