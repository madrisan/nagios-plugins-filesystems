dnl Process this file with autoconf to produce a configure script.

m4_define(PROG_VERSION, 1)
m4_define(PROG_NAME, [nagios-plugins-filesystems])
m4_define(PROG_BUGREPORT, [davide.madrisan@gmail.com])

AC_INIT([PROG_NAME],[PROG_VERSION],[PROG_BUGREPORT],[PROG_NAME])
AC_PREREQ([2.60])

AC_CONFIG_SRCDIR([src/check_readonlyfs.c])
AC_CONFIG_LIBOBJ_DIR([lib])
AC_CONFIG_HEADERS(config.h:config.hin)

AM_INIT_AUTOMAKE([gnu dist-bzip2])

AM_MAINTAINER_MODE
AC_CANONICAL_HOST

AC_SUBST(WARNINGS)

dnl Checks for programs
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AC_PROG_RANLIB

AC_C_INLINE

dnl Checks for header files
AC_HEADER_STDC

# Determine how to get the list of mounted file systems.
ac_list_mounted_fs=

AC_FUNC_GETMNTENT

if test -z "$ac_list_mounted_fs"; then
  # AIX.
  AC_MSG_CHECKING([for mntctl function and struct vmount])
  AC_CACHE_VAL([fu_cv_sys_mounted_vmount],
  [AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <fshelp.h>]])],
    [fu_cv_sys_mounted_vmount=yes],
    [fu_cv_sys_mounted_vmount=no])])
  AC_MSG_RESULT([$fu_cv_sys_mounted_vmount])
  if test $fu_cv_sys_mounted_vmount = yes; then
    ac_list_mounted_fs=found
    AC_DEFINE([MOUNTED_VMOUNT], [1],
        [Define if there is a function named mntctl that can be used to read
         the list of mounted file systems, and there is a system header file
         that declares 'struct vmount'.  (AIX)])
  fi
fi

if test $ac_cv_func_getmntent = yes; then

  # This system has the getmntent function.
  # Determine whether it's the one-argument variant or the two-argument one.

  if test -z "$ac_list_mounted_fs"; then
    # 4.3BSD, SunOS, HP-UX, Dynix, Irix
    AC_MSG_CHECKING([for one-argument getmntent function])
    AC_CACHE_VAL([fu_cv_sys_mounted_getmntent1],
                 [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
/* SunOS 4.1.x /usr/include/mntent.h needs this for FILE */
#include <stdio.h>

#include <mntent.h>
#if !defined MOUNTED
# if defined _PATH_MOUNTED      /* GNU libc  */
#  define MOUNTED _PATH_MOUNTED
# endif
# if defined MNT_MNTTAB /* HP-UX.  */
#  define MOUNTED MNT_MNTTAB
# endif
# if defined MNTTABNAME /* Dynix.  */
#  define MOUNTED MNTTABNAME
# endif
#endif
]],
                      [[ struct mntent *mnt = 0; char *table = MOUNTED;
                         if (sizeof mnt && sizeof table) return 0;]])],
                    [fu_cv_sys_mounted_getmntent1=yes],
                    [fu_cv_sys_mounted_getmntent1=no])])
    AC_MSG_RESULT([$fu_cv_sys_mounted_getmntent1])
    if test $fu_cv_sys_mounted_getmntent1 = yes; then
      ac_list_mounted_fs=found
      AC_DEFINE([MOUNTED_GETMNTENT1], [1],
        [Define if there is a function named getmntent for reading the list
         of mounted file systems, and that function takes a single argument.
         (4.3BSD, SunOS, HP-UX, Dynix, Irix)])
    fi
  fi

  if test -z "$ac_list_mounted_fs"; then
    # SVR4
    AC_MSG_CHECKING([for two-argument getmntent function])
    AC_CACHE_VAL([fu_cv_sys_mounted_getmntent2],
    [AC_EGREP_HEADER([getmntent], [sys/mnttab.h],
      fu_cv_sys_mounted_getmntent2=yes,
      fu_cv_sys_mounted_getmntent2=no)])
    AC_MSG_RESULT([$fu_cv_sys_mounted_getmntent2])
    if test $fu_cv_sys_mounted_getmntent2 = yes; then
      ac_list_mounted_fs=found
      AC_DEFINE([MOUNTED_GETMNTENT2], [1],
        [Define if there is a function named getmntent for reading the list of
         mounted file systems, and that function takes two arguments.  (SVR4)])
      AC_CHECK_FUNCS([hasmntopt])
    fi
  fi

fi

AC_CHECK_HEADERS(getopt.h err.h)
AC_MSG_CHECKING([for struct option in getopt])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([[#include <getopt.h>]],
    [[struct option op; op.name;]])],
  [ac_cv_struct_option_getopt_h=yes],
  [ac_cv_struct_option_getopt_h=no])
AC_MSG_RESULT([$ac_cv_struct_option_getopt_h])

AC_CHECK_HEADERS([sys/mntent.h mntent.h])

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

AC_CONFIG_FILES([
   Makefile
   lib/Makefile
   src/Makefile
])
AC_OUTPUT
