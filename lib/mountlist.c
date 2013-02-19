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

/* This library was largely based on and inspired by the coreutils code.
 */

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

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

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

#ifdef MOUNTED_GETMNTINFO	/* 4.4BSD.  */
# include <sys/mount.h>
#endif

#ifdef MOUNTED_VMOUNT		/* AIX.  */
# include <fshelp.h>
# include <sys/vfs.h>
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

#if MOUNTED_GETMNTINFO

# if ! HAVE_STRUCT_STATFS_F_FSTYPENAME
static char *
fstype_to_string (short int t)
{
  switch (t)
    {
#  ifdef MOUNT_PC
    case MOUNT_PC:
      return "pc";
#  endif
#  ifdef MOUNT_MFS
    case MOUNT_MFS:
      return "mfs";
#  endif
#  ifdef MOUNT_LO
    case MOUNT_LO:
      return "lo";
#  endif
#  ifdef MOUNT_TFS
    case MOUNT_TFS:
      return "tfs";
#  endif
#  ifdef MOUNT_TMP
    case MOUNT_TMP:
      return "tmp";
#  endif
#  ifdef MOUNT_UFS
   case MOUNT_UFS:
     return "ufs" ;
#  endif
#  ifdef MOUNT_NFS
   case MOUNT_NFS:
     return "nfs" ;
#  endif
#  ifdef MOUNT_MSDOS
   case MOUNT_MSDOS:
     return "msdos" ;
#  endif
#  ifdef MOUNT_LFS
   case MOUNT_LFS:
     return "lfs" ;
#  endif
#  ifdef MOUNT_LOFS
   case MOUNT_LOFS:
     return "lofs" ;
#  endif
#  ifdef MOUNT_FDESC
   case MOUNT_FDESC:
     return "fdesc" ;
#  endif
#  ifdef MOUNT_PORTAL
   case MOUNT_PORTAL:
     return "portal" ;
#  endif
#  ifdef MOUNT_NULL
   case MOUNT_NULL:
     return "null" ;
#  endif
#  ifdef MOUNT_UMAP
   case MOUNT_UMAP:
     return "umap" ;
#  endif
#  ifdef MOUNT_KERNFS
   case MOUNT_KERNFS:
     return "kernfs" ;
#  endif
#  ifdef MOUNT_PROCFS
   case MOUNT_PROCFS:
     return "procfs" ;
#  endif
#  ifdef MOUNT_AFS
   case MOUNT_AFS:
     return "afs" ;
#  endif
#  ifdef MOUNT_CD9660
   case MOUNT_CD9660:
     return "cd9660" ;
#  endif
#  ifdef MOUNT_UNION
   case MOUNT_UNION:
     return "union" ;
#  endif
#  ifdef MOUNT_DEVFS
   case MOUNT_DEVFS:
     return "devfs" ;
#  endif
#  ifdef MOUNT_EXT2FS
   case MOUNT_EXT2FS:
     return "ext2fs" ;
#  endif
    default:
      return "?";
    }
}
# endif

static char *
fsp_to_string (const struct statfs *fsp)
{
# if HAVE_STRUCT_STATFS_F_FSTYPENAME
  return (char *) (fsp->f_fstypename);
# else
  return fstype_to_string (fsp->f_type);
# endif
}

/* Map from mount options to printable formats. */
static struct opt
{
  int o_opt;
  const char *o_optname;
} optnames[] = {
# ifdef MNT_ASYNC
  {MNT_ASYNC, "async"},
# endif
# ifdef MNT_LOCAL
  {MNT_LOCAL, "local"},
# endif
# ifdef MNT_NOATIME
  {MNT_NOATIME, "noatime"},
# endif
# ifdef MNT_NODEV
  {MNT_NODEV, "nodev"},
# endif
# ifdef MNT_NOEXEC
  {MNT_NOEXEC, "noexec"},
# endif
# ifdef MNT_NOSUID
  {MNT_NOSUID, "nosuid"},
# endif
# ifdef MNT_RDONLY
  {MNT_RDONLY, "read-only"},
# endif
# ifdef MNT_SYNCHRONOUS
  {MNT_SYNCHRONOUS, "sync"},
# endif
# ifdef MNT_SOFTDEP
  {MNT_SOFTDEP, "softdep"},
# endif
  {0, ""}
};

char *
catopt(char *s0, const char *s1)
{
  size_t i;
  char *cp;

  if (s0 && *s0)
    {
      i = strlen(s0) + strlen(s1) + 1 + 1;
      if ((cp = malloc(i)) == NULL)
	err(1, NULL);
      (void)snprintf(cp, i, "%s,%s", s0, s1);
    }
  else
    cp = strdup(s1);

  free(s0);
  return (cp);
}

static char *
fsp_flags_to_string (u_int32_t f_flags)
{
  char *optlist = NULL;
  struct opt *p;

  for (p = optnames; p->o_opt; p++)
    {
      if (f_flags & p->o_opt && *p->o_optname)
	optlist = catopt (optlist, p->o_optname);
    } 

  return optlist;
}

#endif /* MOUNTED_GETMNTINFO */

#ifdef MOUNTED_VMOUNT		/* AIX.  */
static char *
fstype_to_string (int t)
{
  struct vfs_ent *e;

  e = getvfsbytype (t);
  if (!e || !e->vfsent_name)
    return "none";
  else
    return e->vfsent_name;
}
#endif /* MOUNTED_VMOUNT */

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

#ifdef MOUNTED_GETMNTINFO	/* 4.4BSD.  */
  {
    struct statfs *fsp;
    int entries;

    entries = getmntinfo (&fsp, MNT_NOWAIT);
    if (entries < 0)
      return NULL;
    for (; entries-- > 0; fsp++)
      {
        char *fs_type = fsp_to_string (fsp);

        me = xmalloc (sizeof *me);
        me->me_devname = xstrdup (fsp->f_mntfromname);
        me->me_mountdir = xstrdup (fsp->f_mntonname);
        me->me_type = fs_type;
        me->me_type_malloced = 0;
        me->me_opts = fsp_flags_to_string (fsp->f_flags);
        me->me_opts_malloced = 1;
        me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
        me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
        me->me_readonly = (fsp->f_flags & MNT_RDONLY);
        me->me_dev = (dev_t) -1;        /* Magic; means not known yet. */

        /* Add to the linked list. */
        *mtail = me;
        mtail = &me->me_next;
      }
  }
#endif /* MOUNTED_GETMNTINFO */

#ifdef MOUNTED_VMOUNT		/* AIX.  */
  {
    int bufsize;
    char *entries, *thisent;
    struct vmount *vmp;
    int n_entries;
    int i;

    /* Ask how many bytes to allocate for the mounted file system info.  */
    if (mntctl (MCTL_QUERY, sizeof bufsize, (struct vmount *) &bufsize) != 0)
      return NULL;
    entries = xmalloc (bufsize);

    /* Get the list of mounted file systems.  */
    n_entries = mntctl (MCTL_QUERY, bufsize, (struct vmount *) entries);
    if (n_entries < 0)
      {
        int saved_errno = errno;
        free (entries);
        errno = saved_errno;
        return NULL;
      }

    for (i = 0, thisent = entries;
         i < n_entries;
         i++, thisent += vmp->vmt_length)
      {
        char *options, *ignore;

        vmp = (struct vmount *) thisent;
        me = xmalloc (sizeof *me);
        if (vmp->vmt_flags & MNT_REMOTE)
          {
            char *host, *dir;

            me->me_remote = 1;
            /* Prepend the remote dirname.  */
            host = thisent + vmp->vmt_data[VMT_HOSTNAME].vmt_off;
            dir = thisent + vmp->vmt_data[VMT_OBJECT].vmt_off;
            me->me_devname = xmalloc (strlen (host) + strlen (dir) + 2);
            strcpy (me->me_devname, host);
            strcat (me->me_devname, ":");
            strcat (me->me_devname, dir);
          }
        else
          {
            me->me_remote = 0;
            me->me_devname = xstrdup (thisent +
                                      vmp->vmt_data[VMT_OBJECT].vmt_off);
          }
        me->me_mountdir = xstrdup (thisent + vmp->vmt_data[VMT_STUB].vmt_off);
        me->me_type = xstrdup (fstype_to_string (vmp->vmt_gfstype));
        me->me_type_malloced = 1;
        options = thisent + vmp->vmt_data[VMT_ARGS].vmt_off;
        me->me_opts = xstrdup (options);
        me->me_opts_malloced = 1;
        ignore = strstr (options, "ignore");
        me->me_dummy = (ignore
                        && (ignore == options || ignore[-1] == ',')
                        && (ignore[sizeof "ignore" - 1] == ','
                            || ignore[sizeof "ignore" - 1] == '\0'));
        me->me_readonly = fs_check_if_readonly (me->me_opts);
        me->me_dev = (dev_t) -1; /* vmt_fsid might be the info we want.  */

        /* Add to the linked list. */
        *mtail = me;
        mtail = &me->me_next;
      }
    free (entries);
  }
#endif /* AIX.  */

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
	if (mount_list->me_opts_malloced)
	  free (mount_list->me_opts);
	free (mount_list);
	mount_list = me;
      }

    errno = saved_errno;
    return NULL;
  }
}
