#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mountlist.h"
#include "xalloc.h"

#ifdef MOUNTED_GETMNTENT1
# include <mntent.h>
# if !defined MOUNTED
#  if defined _PATH_MOUNTED	/* GNU libc  */
#   define MOUNTED _PATH_MOUNTED
#  endif
# endif
#endif

#ifdef MOUNTED_GETMNTENT2	/* SVR4.  */
# include <sys/mnttab.h>
#endif

#if HAVE_SYS_MNTENT_H
/* This is to get MNTOPT_IGNORE on e.g. SVR4.  */
# include <sys/mntent.h>
#endif

#undef MNT_IGNORE
#if defined MNTOPT_IGNORE && defined HAVE_HASMNTOPT
# define MNT_IGNORE(M) hasmntopt (M, MNTOPT_IGNORE)
#else
# define MNT_IGNORE(M) 0
#endif

#ifndef ME_DUMMY
# define ME_DUMMY(Fs_name, Fs_type)             \
    (strcmp (Fs_type, "autofs") == 0            \
     || strcmp (Fs_type, "binfmt_misc") == 0    \
     || strcmp (Fs_type, "devpts") == 0         \
     || strcmp (Fs_type, "fusectl") == 0        \
     || strcmp (Fs_type, "none") == 0           \
     || strcmp (Fs_type, "proc") == 0           \
     || strcmp (Fs_type, "subfs") == 0          \
     /* for NetBSD 3.0 */                       \
     || strcmp (Fs_type, "kernfs") == 0         \
     /* for Irix 6.5 */                         \
     || strcmp (Fs_type, "ignore") == 0)
#endif

#ifndef ME_REMOTE
/* A file system is "remote" if its Fs_name contains a ':'
 *    or if (it is of type (smbfs or cifs) and its Fs_name starts with '//').  */
# define ME_REMOTE(Fs_name, Fs_type)            \
    (strchr (Fs_name, ':') != NULL              \
     || ((Fs_name)[0] == '/'                    \
         && (Fs_name)[1] == '/'                 \
         && (strcmp (Fs_type, "smbfs") == 0     \
             || strcmp (Fs_type, "cifs") == 0)))
#endif

/* Check for the "ro" pattern in the MOUNT_OPTIONS.
 *    Return true if found, Otherwise return false.  */
static bool
fs_check_if_readonly (char *mount_options)
{
  static char const readonly_pattern[] = "ro";
  char *str1, *token, *saveptr1;
  int j;

  for (j = 1, str1 = mount_options;; j++, str1 = NULL)
    {
      token = strtok_r (str1, ",", &saveptr1);
      if (token == NULL)
	break;
      if (strcmp (token, readonly_pattern) == 0)
	return true;
    }

  return false;
}

#if defined MOUNTED_GETMNTENT1 || defined MOUNTED_GETMNTENT2

/* Return the device number from MOUNT_OPTIONS, if possible.
 *    Otherwise return (dev_t) -1.  */
static dev_t
dev_from_mount_options (char const *mount_options)
{
  /* GNU/Linux allows file system implementations to define their own
   *      meaning for "dev=" mount options, so don't trust the meaning
   *           here.  */
# ifndef __linux__

  static char const dev_pattern[] = ",dev=";
  char const *devopt = strstr (mount_options, dev_pattern);

  if (devopt)
    {
      char const *optval = devopt + sizeof dev_pattern - 1;
      char *optvalend;
      unsigned long int dev;
      errno = 0;
      dev = strtoul (optval, &optvalend, 16);
      if (optval != optvalend
	  && (*optvalend == '\0' || *optvalend == ',')
	  && !(dev == ULONG_MAX && errno == ERANGE) && dev == (dev_t) dev)
	return dev;
    }

# endif
  (void) mount_options;
  return -1;
}

#endif

/* Return a list of the currently mounted file systems, or NULL on error.
   Add each entry to the tail of the list so that they stay in order.
   If NEED_FS_TYPE is true, ensure that the file system type fields in
   the returned list are valid.  Otherwise, they might not be.  */

struct mount_entry *
read_file_system_list (bool need_fs_type)
{
  struct mount_entry *mount_list;
  struct mount_entry *me;
  struct mount_entry **mtail = &mount_list;
  (void) need_fs_type;

#ifdef MOUNTED_GETMNTENT1	/* GNU/Linux, 4.3BSD, SunOS, HP-UX, Dynix, Irix.  */
  {
    struct mntent *mnt;
    char const *table = MOUNTED;
    FILE *fp;

    fp = setmntent (table, "r");
    if (fp == NULL)
      return NULL;

    while ((mnt = getmntent (fp)))
      {
	me = xmalloc (sizeof *me);
	me->me_devname = xstrdup (mnt->mnt_fsname);
	me->me_mountdir = xstrdup (mnt->mnt_dir);
	me->me_type = xstrdup (mnt->mnt_type);
	me->me_type_malloced = 1;
	me->me_opts = xstrdup (mnt->mnt_opts);
	me->me_opts_malloced = 1;
	me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
	me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
	me->me_readonly = fs_check_if_readonly (me->me_opts);
	me->me_dev = dev_from_mount_options (mnt->mnt_opts);

	/* Add to the linked list. */
	*mtail = me;
	mtail = &me->me_next;
      }

    if (endmntent (fp) == 0)
      goto free_then_fail;
  }
#endif /* MOUNTED_GETMNTENT1. */

#ifdef MOUNTED_GETMNTENT2	/* SVR4.  */
  {
    struct mnttab mnt;
    char *table = MNTTAB;
    FILE *fp;
    int ret;
    int lockfd = -1;

# if defined F_RDLCK && defined F_SETLKW
    /* MNTTAB_LOCK is a macro name of our own invention; it's not present in
       e.g. Solaris 2.6.  If the SVR4 folks ever define a macro
       for this file name, we should use their macro name instead.
       (Why not just lock MNTTAB directly?  We don't know.)  */
#  ifndef MNTTAB_LOCK
#   define MNTTAB_LOCK "/etc/.mnttab.lock"
#  endif
    lockfd = open (MNTTAB_LOCK, O_RDONLY);
    if (0 <= lockfd)
      {
	struct flock flock;
	flock.l_type = F_RDLCK;
	flock.l_whence = SEEK_SET;
	flock.l_start = 0;
	flock.l_len = 0;
	while (fcntl (lockfd, F_SETLKW, &flock) == -1)
	  if (errno != EINTR)
	    {
	      int saved_errno = errno;
	      close (lockfd);
	      errno = saved_errno;
	      return NULL;
	    }
      }
    else if (errno != ENOENT)
      return NULL;
# endif

    errno = 0;
    fp = fopen (table, "r");
    if (fp == NULL)
      ret = errno;
    else
      {
	while ((ret = getmntent (fp, &mnt)) == 0)
	  {
	    me = xmalloc (sizeof *me);
	    me->me_devname = xstrdup (mnt.mnt_special);
	    me->me_mountdir = xstrdup (mnt.mnt_mountp);
	    me->me_type = xstrdup (mnt.mnt_fstype);
	    me->me_type_malloced = 1;
	    me->me_opts = xstrdup (mnt.mnt_mntopts);
	    me->me_opts_malloced = 1;
	    me->me_dummy = MNT_IGNORE (&mnt) != 0;
	    me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
	    me->me_readonly = fs_check_if_readonly (me->me_opts);
	    me->me_dev = dev_from_mount_options (mnt.mnt_mntopts);

	    /* Add to the linked list. */
	    *mtail = me;
	    mtail = &me->me_next;
	  }

	ret = fclose (fp) == EOF ? errno : 0 < ret ? 0 : -1;
      }

    if (0 <= lockfd && close (lockfd) != 0)
      ret = errno;

    if (0 <= ret)
      {
	errno = ret;
	goto free_then_fail;
      }
  }
#endif /* MOUNTED_GETMNTENT2.  */

  *mtail = NULL;
  return mount_list;


free_then_fail:
  {
    int saved_errno = errno;
    *mtail = NULL;

    while (mount_list)
      {
	me = mount_list->me_next;
	free (mount_list->me_devname);
	free (mount_list->me_mountdir);
	if (mount_list->me_type_malloced)
	  free (mount_list->me_type);
	free (mount_list);
	mount_list = me;
      }

    errno = saved_errno;
    return NULL;
  }
}
