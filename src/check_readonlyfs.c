/*
 * License: GPL
 * Copyright (c) 2013 Davide Madrisan <davide.madrisan@gmail.com>
 *
 * A Nagios plugin to check for readonly filesystems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#if HAVE_GETOPT_H
#include <getopt.h>
#else
#include <compat_getopt.h>
#endif

#if HAVE_MNTENT_H
#include <mntent.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mountlist.h"
#include "nputils.h"

/* Linked list of mounted file systems. */
static struct mount_entry *mount_list;

/* Linked list of file system types to display.
 * If 'fs_select_list' is NULL, list all types.
 * This table is generated dynamically from command-line options,
 * rather than hardcoding into the program what it thinks are the
 * valid file system types; let the user specify any file system type
 * they want to, and if there are any file systems of that type, they
 * will be shown.
 *
 * Some file system types:
 * 4.2 4.3 ufs nfs swap ignore io vm efs dbg */

static struct fs_type_list *fs_select_list;

/* Linked list of file system types to omit.
 *    If the list is empty, don't exclude any types.  */

static struct fs_type_list *fs_exclude_list;

/* If true, show only local file systems.  */
static bool show_local_fs;

/* If true, print file system type as well.  */
static bool print_type;

static void __attribute__ ((__noreturn__)) print_version (void)
{
  puts (PACKAGE_NAME " version " PACKAGE_VERSION);
  exit (STATE_OK);
}

static struct option const longopts[] = {
  {(char *) "help", no_argument, NULL, 'h'},
  {(char *) "version", no_argument, NULL, 'V'},
  {(char *) "list", no_argument, NULL, 'L'},
  {NULL, 0, NULL, 0}
};

static void __attribute__ ((__noreturn__)) usage (FILE * out)
{
  fputs (PACKAGE_NAME " ver." PACKAGE_VERSION " - \
check for readonly filesystems\n\
Copyright (C) 2013 Davide Madrisan <" PACKAGE_BUGREPORT ">\n", out);
  fputs ("\n\
  Usage:\n\
\t" PACKAGE_NAME " --list\n\
\t" PACKAGE_NAME " --help\n\
\t" PACKAGE_NAME " --version\n\n", out);

  exit (out == stderr ? STATE_UNKNOWN : STATE_OK);
}

int
main (int argc, char **argv)
{
  int c, status = STATE_UNKNOWN;
  struct mount_entry *me;
  bool print_mountedfs = false;
  fs_exclude_list = NULL;
  print_type = false;

  while ((c = getopt_long (argc, argv, "LhV", longopts, NULL)) != -1)
    {
      switch (c)
	{
	default:
	  usage (stderr);
	  break;
	case 'L':
	  print_mountedfs = true;
	  break;
	case 'h':
	  usage (stdout);
	  break;
	case 'V':
	  print_version ();
	  break;
	}
    }

  mount_list =
    read_file_system_list ((fs_select_list != NULL
			    || fs_exclude_list != NULL
			    || print_type || show_local_fs));

  if (NULL == mount_list)
    {
      /* Couldn't read the table of mounted file systems. */
      perror ("cannot read table of mounted file systems");
    }

  if (print_mountedfs)
    {
      me = mount_list;
      fprintf (stdout, "List of checked filesystems:\n");
      while (me)
	{
	  fprintf (stdout, " %s (%s)\n", me->me_mountdir, me->me_type);
	  me = me->me_next;
	}
    }

  return status;
}
