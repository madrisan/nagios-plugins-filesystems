AM_CFLAGS = @WARNINGS@

noinst_LIBRARIES = libfilesystems.a
libfilesystems_a_SOURCES = mountlist.c xalloc.h xmalloc.c nputils.h compat_getopt.h
libfilesystems_a_LIBADD = $(LIBOBJS)

libexec_PROGRAMS = check_readonlyfs

check_readonlyfs_SOURCES = check_readonlyfs.c mountlist.c xmalloc.c
check_readonlyfs_LDADD = libfilesystems.a
