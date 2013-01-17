#include "config.h"

#ifdef MOUNTED_GETMNTENT1
# include <mntent.h>
# if !defined MOUNTED
#  if defined _PATH_MOUNTED	/* GNU libc  */
#   define MOUNTED _PATH_MOUNTED
#  endif
# endif
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mountlist.h"
#include "xalloc.h"

#ifndef ME_DUMMY
# define ME_DUMMY(Fs_name, Fs_type)             \
    (strcmp (Fs_type, "autofs") == 0            \
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

#ifndef ME_READONLY
# define ME_READONLY(Fs_name, Fs_opts)          \
    (strcmp (Fs_opts, "ro") == 0)
#endif

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
 *    Add each entry to the tail of the list so that they stay in order.
 *       If NEED_FS_TYPE is true, ensure that the file system type fields in
 *          the returned list are valid.  Otherwise, they might not be.  */
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
	me->me_opts = xstrdup (mnt->mnt_opts);
	me->me_type_malloced = me->me_opts_malloced = 1;
	me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
	me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
	me->me_readonly = ME_READONLY (me->me_devname, me->me_opts);
	me->me_dev = dev_from_mount_options (mnt->mnt_opts);

	/* Add to the linked list. */
	*mtail = me;
	mtail = &me->me_next;
      }

    if (endmntent (fp) == 0)
      goto free_then_fail;
  }
#endif /* MOUNTED_GETMNTENT1. */

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
