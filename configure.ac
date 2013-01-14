dnl Process this file with autoconf to produce a configure script.

m4_define(PROG_VERSION, 0)
m4_define(PROG_NAME, [nagios-plugins-filesystems])
m4_define(PROG_BUGREPORT, [davide.madrisan@gmail.com])

AC_INIT([PROG_NAME],[PROG_VERSION],[PROG_BUGREPORT],[PROG_NAME])
AC_PREREQ([2.59])

AC_CONFIG_SRCDIR([src/check_readonlyfs.c])
#AC_CONFIG_LIBOBJ_DIR([lib])
AC_CONFIG_HEADERS(config.h:config.hin)

AM_INIT_AUTOMAKE([gnu dist-bzip2])

AM_MAINTAINER_MODE
AC_CANONICAL_HOST

AC_SUBST(WARNINGS)

dnl Checks for programs
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AC_PROG_RANLIB

dnl Checks for header files
AC_HEADER_STDC
AC_HEADER_TIME

AC_CHECK_HEADERS(getopt.h err.h)
AC_MSG_CHECKING([for struct option in getopt])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([[#include <getopt.h>]],
    [[struct option op; op.name;]])],
  [ac_cv_struct_option_getopt_h=yes],
  [ac_cv_struct_option_getopt_h=no])
AC_MSG_RESULT([$ac_cv_struct_option_getopt_h])

AC_PREFIX_DEFAULT(/usr/local/nagios)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_PROTOTYPES

AC_MSG_CHECKING(for gcc compiler warnings)
AC_ARG_ENABLE(warnings,
[  --disable-warnings   disable compiler warnings],
[if test "${enableval}" = no -o "$GCC" = no; then
   AC_MSG_RESULT(no)
   WARNINGS=
else
   AC_MSG_RESULT(yes)
   WARNINGS="\
-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
-Wmissing-declarations -Wredundant-decls -Wnested-externs \
-Wsign-compare -Wreturn-type -Wswitch -Wshadow \
-Wwrite-strings -Wextra -Wunused-parameter -Wformat=2"
fi], [
if test "$GCC" = yes; then
   AC_MSG_RESULT(yes)
   WARNINGS="\
-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
-Wmissing-declarations -Wredundant-decls -Wnested-externs \
-Wsign-compare -Wreturn-type -Wswitch -Wshadow \
-Wwrite-strings -Wextra -Wunused-parameter -Wformat=2"
else
   WARNINGS=
   AC_MSG_RESULT(no)
fi])

dnl Provide implementation of some required functions if necessary
AC_REPLACE_FUNCS(getopt_long)

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
