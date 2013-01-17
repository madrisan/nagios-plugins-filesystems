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
#include <string.h>
#include <unistd.h>

#include "mountlist.h"
#include "nputils.h"
#include "xalloc.h"

#define STREQ(a, b) (strcmp (a, b) == 0)

/* A file system type to display. */

struct fs_type_list
{
  char *fs_name;
  struct fs_type_list *fs_next;
};

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

/* Linked list of mounted file systems. */
static struct mount_entry *mount_list;

/* If true, show only local file systems.  */
static bool show_local_fs;

/* If true, show each file system corresponding to the
   command line arguments.  */
static bool show_listed_fs;

static void __attribute__ ((__noreturn__)) print_version (void)
{
  puts (PACKAGE_NAME " version " PACKAGE_VERSION);
  exit (STATE_OK);
}

static struct option const longopts[] = {
  {(char *) "help", no_argument, NULL, 'h'},
  {(char *) "version", no_argument, NULL, 'V'},
  {(char *) "local", no_argument, NULL, 'l'},
  {(char *) "list", no_argument, NULL, 'L'},
  {(char *) "type", required_argument, NULL, 'T'},
  {(char *) "exclude-type", required_argument, NULL, 'X'},
  {NULL, 0, NULL, 0}
};

/* Add FSTYPE to the list of file system types to display. */

static void
add_fs_type (const char *fstype)
{
  struct fs_type_list *fsp;

  fsp = xmalloc (sizeof *fsp);
  fsp->fs_name = (char *) fstype;
  fsp->fs_next = fs_select_list;
  fs_select_list = fsp;
}

/* Is FSTYPE a type of file system that should be listed?  */

static bool
selected_fstype (const char *fstype)
{
  const struct fs_type_list *fsp;

  if (fs_select_list == NULL || fstype == NULL)
    return true;
  for (fsp = fs_select_list; fsp; fsp = fsp->fs_next)
    if (STREQ (fstype, fsp->fs_name))
      return true;
  return false;
}

/* Add FSTYPE to the list of file system types to be omitted. */

static void
add_excluded_fs_type (const char *fstype)
{
  struct fs_type_list *fsp;

  fsp = xmalloc (sizeof *fsp);
  fsp->fs_name = (char *) fstype;
  fsp->fs_next = fs_exclude_list;
  fs_exclude_list = fsp;
}

/* Is FSTYPE a type of file system that should be omitted?  */

static bool
excluded_fstype (const char *fstype)
{
  const struct fs_type_list *fsp;

  if (fs_exclude_list == NULL || fstype == NULL)
    return false;
  for (fsp = fs_exclude_list; fsp; fsp = fsp->fs_next)
    if (STREQ (fstype, fsp->fs_name))
      return true;
  return false;
}

static void __attribute__ ((__noreturn__)) usage (FILE * out)
{
  fputs (PACKAGE_NAME " ver." PACKAGE_VERSION " - \
check for readonly filesystems\n\
Copyright (C) 2013 Davide Madrisan <" PACKAGE_BUGREPORT ">\n", out);
  fputs ("\n\
Usage: " PACKAGE_NAME " [OPTION]... [FILE]...\n\n", out);
  fputs ("\
Mandatory arguments to long options are mandatory for short options too.\n", stdout);
  fputs ("\
  -l, --local               limit listing to local file systems\n\
  -L, --list                display the list of checked file systems\n\
  -T, --type=TYPE           limit listing to file systems of type TYPE\n\
  -X, --exclude-type=TYPE   limit listing to file systems not of type TYPE\n\
  -h, --help                display this help and exit\n\
  -v, --version             output version information and exit\n", out);

  exit (out == stderr ? STATE_UNKNOWN : STATE_OK);
}

int
main (int argc, char **argv)
{
  int c, status = STATE_OK;
  struct mount_entry *me, *meprev;

  fs_select_list = NULL;
  fs_exclude_list = NULL;

  while ((c = getopt_long (argc, argv, "lLT:X:hV", longopts, NULL)) != -1)
    {
      switch (c)
	{
	default:
	  usage (stderr);
	  break;
	case 'l':
	  show_local_fs = true;
	  break;
	case 'L':
	  show_listed_fs = true;
	  break;
	case 'T':
	  add_fs_type (optarg);
	  break;
	case 'X':
	  add_excluded_fs_type (optarg);
	  break;
	case 'h':
	  usage (stdout);
	  break;
	case 'V':
	  print_version ();
	  break;
	}
    }

  /* Fail if the same file system type was both selected and excluded.  */
  {
    bool match = false;
    struct fs_type_list *fs_incl;
    for (fs_incl = fs_select_list; fs_incl; fs_incl = fs_incl->fs_next)
      {
	struct fs_type_list *fs_excl;
	for (fs_excl = fs_exclude_list; fs_excl; fs_excl = fs_excl->fs_next)
	  {
	    if (STREQ (fs_incl->fs_name, fs_excl->fs_name))
	      {
		fprintf (stderr,
			 "file system type %s both selected and excluded\n",
			 fs_incl->fs_name);
		match = true;
		break;
	      }
	  }
      }
    if (match)
      return STATE_UNKNOWN;
  }

  mount_list =
    read_file_system_list ((fs_select_list != NULL
			    || fs_exclude_list != NULL || show_local_fs));

  if (NULL == mount_list)
    {
      /* Couldn't read the table of mounted file systems. */
      fprintf (stderr, "cannot read table of mounted file systems");
      return STATE_UNKNOWN;
    }

  me = mount_list;
  while (me)
    {
      if ((excluded_fstype (me->me_type) == false)
	  && (selected_fstype (me->me_type) == true))
	{
	  if ((show_local_fs && !me->me_remote) || !show_local_fs)
	    {
	      if (show_listed_fs)
		fprintf (stdout, " %s (%s) %s%s\n", me->me_mountdir,
			 me->me_type, me->me_opts,
			 (me->me_readonly) ? " *** readonly! ***" : "");
	      else if (me->me_readonly)
		{
		  fprintf (stderr, "%s is readonly!\n", me->me_mountdir);	/* FIXME */
		  status = STATE_CRITICAL;
		}
	    }
	}
      meprev = me;
      me = me->me_next;
      if (meprev->me_type_malloced)
	free (meprev->me_type);
      if (meprev->me_opts_malloced)
	free (meprev->me_opts);
      free (meprev);
    }
  free (mount_list);

  /* free 'fs_exclude_list' */
  struct fs_type_list *fsp = fs_exclude_list, *next;
  while (fsp)
    {
      next = fsp->fs_next;
      free (fsp);
      fsp = next;
    }

  return status;
}
