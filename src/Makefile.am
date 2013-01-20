AM_CFLAGS = @WARNINGS@

noinst_LIBRARIES = libfilesystems.a
libfilesystems_a_SOURCES = \
   error.c mountlist.c \
   error.h xalloc.h xmalloc.c nputils.h compat_getopt.h
libfilesystems_a_LIBADD = $(LIBOBJS)

libexec_PROGRAMS = check_readonlyfs

check_readonlyfs_SOURCES = check_readonlyfs.c
check_readonlyfs_LDADD = libfilesystems.a
