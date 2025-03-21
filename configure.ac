# This file is part of GNU paxutils
#
# Copyright (C) 2005-2024 Free Software Foundation, Inc.
#
# GNU paxutils is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 3, or (at your option) any later
# version.
#
# GNU paxutils program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with GNU paxutils.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([GNU paxutils], [0.0.1], [bug-paxutils@gnu.org])
AC_CONFIG_SRCDIR([paxtest/paxtest.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AC_PREREQ([2.70])
AM_INIT_AUTOMAKE([1.8 gnits dist-bzip2 dist-shar std-options])

AC_PROG_CC
AC_EXEEXT
gl_EARLY
AC_PROG_RANLIB
AC_SYS_LARGEFILE

AC_HEADER_MAJOR

AC_MSG_CHECKING([for st_fstype string in struct stat])
AC_CACHE_VAL(diff_cv_st_fstype_string,
  [AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
        [[#include <sys/types.h>
	  #include <sys/stat.h>
	]],
	[[struct stat s; s.st_fstype[0] = 'x';]])],
     [diff_cv_st_fstype_string=yes],
     [diff_cv_st_fstype_string=no])])
AC_MSG_RESULT($diff_cv_st_fstype_string)
if test $diff_cv_st_fstype_string = yes; then
  AC_DEFINE(HAVE_ST_FSTYPE_STRING, 1,
    [Define if struct stat has a char st_fstype[] member.])
fi

AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_CHECK_TYPE(major_t, , AC_DEFINE(major_t, int,
				   [Type of major device numbers.]))
AC_CHECK_TYPE(minor_t, , AC_DEFINE(minor_t, int,
				   [Type of minor device numbers.]))
AC_CHECK_TYPE(dev_t, unsigned)
AC_CHECK_TYPE(ino_t, unsigned)

gl_INIT

PU_RMT
PU_RTAPELIB
PU_SYSTEM

AC_CACHE_CHECK(for remote shell, tar_cv_path_RSH,
  [if test -n "$RSH"; then
    tar_cv_path_RSH=$RSH
  else
    tar_cv_path_RSH=no
    for ac_file in /usr/ucb/rsh /usr/bin/remsh /usr/bin/rsh /usr/bsd/rsh \
	/usr/bin/nsh /usr/bin/rcmd
    do
      # Prefer a non-symlink rsh to a symlink one, so that binaries built
      # on AIX 4.1.4, where /usr/ucb/rsh is a symlink to /usr/bin/rsh
      # will run on AIX 4.3.0, which has only /usr/bin/rsh.
      if test -f $ac_file; then
	if (test -h $ac_file) 2>/dev/null; then
	  test $tar_cv_path_RSH = no && tar_cv_path_RSH=$ac_file
	else
	  tar_cv_path_RSH=$ac_file
	  break
	fi
      fi
    done
  fi])
if test $tar_cv_path_RSH != no; then
  AC_DEFINE_UNQUOTED(REMOTE_SHELL, "$tar_cv_path_RSH",
    [Define to the full path of your rsh, if any.])
fi

AC_MSG_CHECKING(for default archive format)

AC_ARG_VAR([DEFAULT_ARCHIVE_FORMAT],
           [Set the default archive format. Allowed values are: V7, OLDGNU, USTAR, POSIX, GNU. Default is GNU])

if test -z "$DEFAULT_ARCHIVE_FORMAT"; then
  DEFAULT_ARCHIVE_FORMAT="GNU"
fi
case $DEFAULT_ARCHIVE_FORMAT in
  V7|OLDGNU|USTAR|POSIX|GNU) ;;
  *) AC_MSG_ERROR(Invalid format name);;
esac
AC_DEFINE_UNQUOTED(DEFAULT_ARCHIVE_FORMAT, ${DEFAULT_ARCHIVE_FORMAT}_FORMAT,
  [By default produce archives of this format])
AC_MSG_RESULT($DEFAULT_ARCHIVE_FORMAT)

AC_MSG_CHECKING(for default archive)

AC_ARG_VAR([DEFAULT_ARCHIVE],
           [Set the name of the default archive (default: -)])
if test -z "$DEFAULT_ARCHIVE"; then
  DEFAULT_ARCHIVE=-
else
  if test -z "`ls $DEFAULT_ARCHIVE 2>/dev/null`"; then
    AC_MSG_WARN([DEFAULT_ARCHIVE '$DEFAULT_ARCHIVE' not found on this system])
  fi
  # FIXME: Look for DEFTAPE in <sys/mtio.h>.
  # FIXME: Let DEVICE_PREFIX be configured from the environment.
  # FIXME: Rearrange, here.
  case $DEFAULT_ARCHIVE in
    *[[0-7][lmh]])
      AC_DEFINE(DENSITY_LETTER, 1,
	[[Define to 1 if density may be indicated by [lmh] at end of device.]])
      device_prefix=`echo $DEFAULT_ARCHIVE | sed 's/[0-7][lmh]$//'`
      ;;
    *[[0-7]])
      device_prefix=`echo $DEFAULT_ARCHIVE | sed 's/[0-7]$//'`
      ;;
    *)
      device_prefix=
      ;;
  esac
  case "$device_prefix" in
    ?*)
      AC_DEFINE_UNQUOTED(DEVICE_PREFIX, "$device_prefix",
	[Define to a string giving the prefix of the default device, without the part specifying the unit and density.])
      ;;
  esac
fi
AC_DEFINE_UNQUOTED(DEFAULT_ARCHIVE, "$DEFAULT_ARCHIVE",
  [Define to a string giving the full name of the default archive file.])
AC_MSG_RESULT($DEFAULT_ARCHIVE)

AC_ARG_VAR([DEFAULT_BLOCKING],
           [Define default blocking factor (default: 20)])
AC_MSG_CHECKING(for default blocking)
DEFAULT_BLOCKING=${DEFAULT_BLOCKING-20}
AC_DEFINE_UNQUOTED(DEFAULT_BLOCKING, $DEFAULT_BLOCKING,
  [Define to a number giving the default blocking size for archives.])
AC_MSG_RESULT($DEFAULT_BLOCKING)

# Gettext.
AM_GNU_GETTEXT([external], [need-ngettext])
AM_GNU_GETTEXT_VERSION([0.22.5])

AC_SUBST(BACKUP_LIBEXEC_SCRIPTS)
AC_SUBST(BACKUP_SBIN_SCRIPTS)
AC_ARG_ENABLE(backup-scripts,
	      AS_HELP_STRING([--enable-backup-scripts],
                             [Create and install backup and restore scripts]),
              [case $enableval in
	       yes) BACKUP_LIBEXEC_SCRIPTS='$(BACKUP_LIBEXEC_SCRIPTS_LIST)'
                    BACKUP_SBIN_SCRIPTS='$(BACKUP_SBIN_SCRIPTS_LIST)'
                    ;;
               esac])

AC_SUBST(BACKUP_SED_COND)
if date +%Y-%m-%d 2>/dev/null >&2; then
	BACKUP_SED_COND='/^\#ELSE_DATE_FORMAT_OK/,/^\#ENDIF_DATE_FORMAT_OK/d;/^\#IF_DATE_FORMAT_OK/d'
else
	BACKUP_SED_COND='/^\#IF_DATE_FORMAT_OK/,/^\#ELSE_DATE_FORMAT_OK/d;/^\#ENDIF_DATE_FORMAT_OK/d'
fi


AC_CONFIG_FILES([Makefile\
           gnu/Makefile\
           paxlib/Makefile\
	   rmt/Makefile\
           po/Makefile.in\
           paxtest/Makefile])
AC_OUTPUT
