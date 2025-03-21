/* This file is part of GNU Paxutils.
   Copyright (C) 2009, 2023-2024 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "system.h"

#if HAVE_SYS_MTIO_H
# include <sys/mtio.h>
#endif

#include <configmake.h>
#include <argp.h>
#include <argp-version-etc.h>
#include <getopt.h>
#include <configmake.h>
#include <error.h>
#include <progname.h>
#include <c-ctype.h>


static long int dbglev;
static FILE *dbgout;

#define DEBUG(lev,msg)						\
  do { if (dbgout && (lev) <= dbglev) fputs (msg, dbgout); }		\
  while (false)
#define DEBUG1(lev, fmt, x)						\
  do { if (dbgout && (lev) <= dbglev) fprintf (dbgout, fmt, x); } while (false)
#define DEBUG2(lev, fmt, x1, x2)					\
  do									\
    {									\
      if (dbgout && (lev) <= dbglev)					\
	fprintf (dbgout, fmt, x1, x2);					\
    }									\
  while (false)

#define VDEBUG(lev, pfx, fmt)		        \
  do						\
    {						\
      if (dbgout && (lev) <= dbglev)		\
	{					\
          va_list aptr;                         \
          va_start (aptr, fmt);                 \
	  fputs (pfx, dbgout);			\
	  vfprintf (dbgout, fmt, aptr);		\
          va_end (aptr);                        \
	}					\
    }						\
  while (false)



static char *input_buf_ptr;
static size_t input_buf_size;

static char *
rmt_read (void)
{
  ssize_t rc = getline (&input_buf_ptr, &input_buf_size, stdin);
  if (rc > 0)
    {
      if (input_buf_ptr[rc - 1] == '\n')
	input_buf_ptr[rc - 1] = '\0';
      DEBUG1 (10, "C: %s", input_buf_ptr);
      return input_buf_ptr;
    }
  DEBUG (10, "reached EOF");
  return nullptr;
}

static void
rmt_write (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stdout, fmt, ap);
  va_end (ap);
  fflush (stdout);
  VDEBUG (10, "S: ", fmt);
}

static void
rmt_reply (uintmax_t code)
{
  rmt_write ("A%ju\n", code);
}

static void
rmt_error_message (int code, const char *msg)
{
  DEBUG1 (10, "S: E%d\n", code);
  DEBUG1 (10, "S: %s\n", msg);
  DEBUG1 (1, "error: %s\n", msg);
  fprintf (stdout, "E%d\n%s\n", code, msg);
  fflush (stdout);
}

static void
rmt_error (int code)
{
  rmt_error_message (code, strerror (code));
}


static char *record_buffer_ptr;
static idx_t record_buffer_size;

static void
prepare_record_buffer (idx_t size)
{
  if (size > record_buffer_size)
    record_buffer_ptr = xpalloc (record_buffer_ptr, &record_buffer_size,
				 size - record_buffer_size, -1, 1);
}



static int device_fd = -1;

struct rmt_kw
{
  char const *name;
  idx_t len;
  int value;
};

#define RMT_KW(s,v) { #s, sizeof (#s) - 1, v }

static bool
xlat_kw (const char *s, const char *pfx,
	 struct rmt_kw const *kw, int *valp, const char **endp)
{
  idx_t slen = strlen (s);

  if (pfx)
    {
      idx_t pfxlen = strlen (pfx);
      if (slen > pfxlen && memcmp (s, pfx, pfxlen) == 0)
	{
	  s += pfxlen;
	  slen -= pfxlen;
	}
    }

  for (; kw->name; kw++)
    {
      if (slen >= kw->len
	  && memcmp (kw->name, s, kw->len) == 0
	  && !(s[kw->len] && c_isalnum (s[kw->len])))
	{
	  *valp = kw->value;
	  *endp = s + kw->len;
	  return true;
	}
    }
  return false;
}

static const char *
skip_ws (const char *s)
{
  while (*s && c_isblank (*s))
    s++;
  return s;
}

static struct rmt_kw const open_flag_kw[] =
  {
#ifdef O_APPEND
    RMT_KW(APPEND, O_APPEND),
#endif
    RMT_KW(CREAT, O_CREAT),
#ifdef O_DSYNC
    RMT_KW(DSYNC, O_DSYNC),
#endif
    RMT_KW(EXCL, O_EXCL),
#ifdef O_LARGEFILE
    RMT_KW(LARGEFILE, O_LARGEFILE),
#endif
#ifdef O_NOCTTY
    RMT_KW(NOCTTY, O_NOCTTY),
#endif
#if O_NONBLOCK
    RMT_KW(NONBLOCK, O_NONBLOCK),
#endif
    RMT_KW(RDONLY, O_RDONLY),
    RMT_KW(RDWR, O_RDWR),
#ifdef O_RSYNC
    RMT_KW(RSYNC, O_RSYNC),
#endif
#ifdef O_SYNC
    RMT_KW(SYNC, O_SYNC),
#endif
    RMT_KW(TRUNC, O_TRUNC),
    RMT_KW(WRONLY, O_WRONLY),
    { nullptr }
  };

static bool
decode_oflags (const char *fstr, int *pflag)
{
  fstr = skip_ws (fstr);

  long int numeric_flag;
  if (c_isdigit (*fstr))
    {
      char *p;
      errno = 0;
      numeric_flag = strtol (fstr, &p, 10);
      if (errno)
	{
	  rmt_error_message (EINVAL, "invalid open flag");
	  return false;
	}
      fstr = skip_ws (p);
    }
  else
    numeric_flag = 0;

  int flag;
  if (*fstr)
    {
      flag = 0;

      while (*(fstr = skip_ws (fstr)))
	{
	  bool err;
	  int v;
	  if (c_isdigit (*fstr))
	    {
	      char *p;
	      errno = 0;
	      uintmax_t uv = strtoumax (fstr, &p, 10);
	      if (errno)
		err = true;
	      else
		{
		  fstr = p;
		  err = ckd_add (&v, uv, 0);
		}
	    }
	  else
	    err = !xlat_kw (fstr, "O_", open_flag_kw, &v, &fstr);

	  if (err)
	    {
	      rmt_error_message (EINVAL, "invalid open flag");
	      return 1;
	    }

	  flag |= v;

	  fstr = skip_ws (fstr);
	  if (!*fstr)
	    break;
	  if (*fstr != '|')
	    {
	      rmt_error_message (EINVAL, "invalid open flag");
	      return false;
	    }

	  fstr++;
	  /* FIXME:
	     if (!*fstr)
	       rmt_error_message (EINVAL, "invalid open flag");
	  */
	}
    }
  else if (ckd_add (&flag, numeric_flag, 0))
    {
      rmt_error_message (EINVAL, "invalid open flag");
      return false;
    }

  *pflag = flag;
  return true;
}


/* Syntax
   ------
   O<device>\n<flags>\n

   Function
   --------
   Opens the <device> with given <flags>. If a device had already been opened,
   it is closed before opening the new one.

   Arguments
   ---------
   <device> - name of the device to open.
   <flags>  - flags for open(2): a decimal number, or any valid O_* constant
   from fcntl.h (the initial O_ may be omitted), or a bitwise or (using '|')
   of any number of these, e.g.:

      576
      64|512
      CREAT|TRUNC

   In addition, a compined form is also allowed, i.e. a decimal mode followed
   by its symbolic representation.  In this case the symbolic representation
   is given preference.

   Reply
   -----
   A0\n on success, E0\n<msg>\n on error.

   Extensions
   ----------
   BSD version allows only decimal number as <flags>
*/

static void
open_device (char *str)
{
  char *device = xstrdup (str);
  char *oflags_str = rmt_read ();
  if (!oflags_str)
    {
      DEBUG (1, "unexpected EOF");
      exit (EXIT_FAILURE);
    }
  int oflags;
  if (decode_oflags (oflags_str, &oflags))
    {
      if (device_fd >= 0)
	close (device_fd);

      device_fd = open (device, oflags, MODE_RW);
      if (device_fd < 0)
	rmt_error (errno);
      else
	rmt_reply (0);
    }
  free (device);
}

/* Syntax
   ------
   C[<device>]\n

   Function
   --------
   Close the currently open device.

   Arguments
   ---------
   Any arguments are silently ignored.

   Reply
   -----
   A0\n on success, E0\n<msg>\n on error.
*/
static void
close_device (void)
{
  if (close (device_fd) < 0)
    rmt_error (errno);
  else
    {
      device_fd = -1;
      rmt_reply (0);
    }
}

/* Syntax
   ------
   L<whence>\n<offset>\n

   Function
   --------
   Perform an lseek(2) on the currently open device with the specified
   parameters.

   Arguments
   ---------
   <whence>  -  Where to measure offset from. Valid values are:
                0, SET, SEEK_SET to seek from the file beginning,
		1, CUR, SEEK_CUR to seek from the current location in file,
		2, END, SEEK_END to seek from the file end.
   Reply
   -----
   A<offset>\n on success. The <offset> is the new offset in file.
   E0\n<msg>\n on error.

   Extensions
   ----------
   BSD version allows only 0,1,2 as <whence>.
*/

static struct rmt_kw const seek_whence_kw[] =
  {
    RMT_KW(SET, SEEK_SET),
    RMT_KW(CUR, SEEK_CUR),
    RMT_KW(END, SEEK_END),
    { nullptr }
  };

static void
lseek_device (const char *str)
{
  char *p;
  char const *cp;
  int whence;
  off_t off;
  uintmax_t n;

  if (str[0] && str[1] == 0)
    {
      switch (str[0])
	{
	case '0':
	  whence = SEEK_SET;
	  break;

	case '1':
	  whence = SEEK_CUR;
	  break;

	case '2':
	  whence = SEEK_END;
	  break;

	default:
	  rmt_error_message (EINVAL, N_("Seek direction out of range"));
	  return;
	}
    }
  else if (!xlat_kw (str, "SEEK_", seek_whence_kw, &whence, &cp))
    {
      rmt_error_message (EINVAL, N_("Invalid seek direction"));
      return;
    }

  str = rmt_read ();
  n = off = strtoumax (str, &p, 10);
  if (*p)
    {
      rmt_error_message (EINVAL, N_("Invalid seek offset"));
      return;
    }

  if (n != off || errno == ERANGE)
    {
      rmt_error_message (EINVAL, N_("Seek offset out of range"));
      return;
    }

  off = lseek (device_fd, off, whence);
  if (off < 0)
    rmt_error (errno);
  else
    rmt_reply (off);
}

/* Syntax
   ------
   R<count>\n

   Function
   --------
   Read <count> bytes of data from the current device.

   Arguments
   ---------
   <count>  -  number of bytes to read.

   Reply
   -----
   On success: A<rdcount>\n, followed by <rdcount> bytes of data read from
   the device.
   On error: E0\n<msg>\n
*/

static void
read_device (const char *str)
{
  char *p;
  errno = 0;
  uintmax_t n = strtoumax (str, &p, 10);
  if (p == str || *p)
    {
      rmt_error_message (EINVAL, N_("Invalid byte count"));
      return;
    }

  idx_t size;
  if (ckd_add (&size, n, 0) || errno == ERANGE)
    {
      rmt_error_message (EINVAL, N_("Byte count out of range"));
      return;
    }

  prepare_record_buffer (size);
  ptrdiff_t status = safe_read (device_fd, record_buffer_ptr, size);
  if (status < 0)
    rmt_error (errno);
  else
    {
      rmt_reply (status);
      full_write (STDOUT_FILENO, record_buffer_ptr, status);
    }
}

/* Syntax
   ------
   W<count>\n followed by <count> bytes of input data.

   Function
   --------
   Write data onto the current device.

   Arguments
   ---------
   <count>  - number of bytes.

   Reply
   -----
   On success: A<wrcount>\n, where <wrcount> is number of bytes actually
   written.
   On error: E0\n<msg>\n
*/

static void
write_device (const char *str)
{
  char *p;
  uintmax_t n = strtoumax (str, &p, 10);
  if (p == str || *p)
    {
      rmt_error_message (EINVAL, N_("Invalid byte count"));
      return;
    }

  idx_t size;
  if (ckd_add (&size, n, 0))
    {
      rmt_error_message (EINVAL, N_("Byte count out of range"));
      return;
    }

  prepare_record_buffer (size);
  if (fread (record_buffer_ptr, size, 1, stdin) != 1)
    {
      if (feof (stdin))
	rmt_error_message (EIO, N_("Premature eof"));
      else
	rmt_error (errno);
      return;
    }

  idx_t status = full_write (device_fd, record_buffer_ptr, size);
  if (status != size)
    rmt_error (errno);
  else
    rmt_reply (status);
}

/* Syntax
   ------
   I<opcode>\n<count>\n

   Function
   --------
   Perform a MTIOCOP ioctl(2) command using the specified paramedters.

   Arguments
   ---------
   <opcode>   -  MTIOCOP operation code.
   <count>    -  mt_count.

   Reply
   -----
   On success: A0\n
   On error: E0\n<msg>\n
*/

static void
iocop_device (const char *str)
{
  char *p;
  uintmax_t opcode = (c_isdigit (*str)
		      ? (errno = 0, strtoumax (str, &p, 10))
		      : (errno = EINVAL, 0));
  if (errno || *p)
    {
      rmt_error_message (EINVAL, N_("Invalid operation code"));
      return;
    }
  str = rmt_read ();
  uintmax_t count = (c_isdigit (*str)
		     ? (errno = 0, strtoumax (str, &p, 10))
		     : (errno = EINVAL, 0));
  if (errno || *p)
    {
      rmt_error_message (EINVAL, N_("Invalid byte count"));
      return;
    }

#ifdef MTIOCTOP
  struct mtop mtop;

  if (ckd_add (&mtop.mt_count, count, 0))
    {
      rmt_error_message (EINVAL, N_("Byte count out of range"));
      return;
    }

  if (ckd_add (&mtop.mt_op, opcode, 0))
    {
      rmt_error_message (EINVAL, N_("Opcode out of range"));
      return;
    }

  if (ioctl (device_fd, MTIOCTOP, (char *) &mtop) < 0)
    rmt_error (errno);
  else
    rmt_reply (0);
#else
  rmt_error_message (ENOSYS, N_("Operation not supported"));
#endif
}

/* Syntax
   ------
   S\n

   Function
   --------
   Return the status of the open device, as obtained with a MTIOCGET
   ioctl call.

   Arguments
   ---------
   None

   Reply
   -----
   On success: A<count>\n followed by <count> bytes of data.
   On error: E0\n<msg>\n
*/

static void
status_device (const char *str)
{
  if (*str)
    {
      rmt_error_message (EINVAL, N_("Unexpected arguments"));
      return;
    }
#ifdef MTIOCGET
  {
    struct mtget mtget;

    if (ioctl (device_fd, MTIOCGET, &mtget) < 0)
      rmt_error (errno);
    else
      {
	rmt_reply (sizeof mtget);
	full_write (STDOUT_FILENO, &mtget, sizeof mtget);
      }
  }
#else
  rmt_error_message (ENOSYS, N_("Operation not supported"));
#endif
}



const char *argp_program_version = "rmt (" PACKAGE_NAME ") " VERSION;
const char *argp_program_bug_address = "<" PACKAGE_BUGREPORT ">";

static char const doc[] = N_("Manipulate a tape drive, accepting commands from a remote process");

enum {
  DEBUG_FILE_OPTION = 256
};

static struct argp_option options[] = {
  { "debug", 'd', N_("NUMBER"), 0,
    N_("set debug level"), 0 },
  { "debug-file", DEBUG_FILE_OPTION, N_("FILE"), 0,
    N_("set debug output file name"), 0 },
  { nullptr }
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'd':
      dbglev = strtol (arg, nullptr, 0);
      break;

    case DEBUG_FILE_OPTION:
      dbgout = fopen (arg, "w");
      if (!dbgout)
	error (EXIT_FAILURE, errno, _("cannot open %s"), arg);
      break;

    case ARGP_KEY_FINI:
      if (dbglev)
	{
	  if (!dbgout)
	    dbgout = stderr;
	}
      else if (dbgout)
	dbglev = 1;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp argp = {
  options,
  parse_opt,
  nullptr,
  doc,
  nullptr,
  nullptr,
  nullptr
};

static const char *rmt_authors[] = {
  "Sergey Poznyakoff",
  nullptr
};


void
xalloc_die (void)
{
  rmt_error (ENOMEM);
  exit (EXIT_FAILURE);
}


int
main (int argc, char **argv)
{
  char *buf;
  int idx;
  bool stop = false;

  set_program_name (argv[0]);
  argp_version_setup ("rmt", rmt_authors);

  if (isatty (STDOUT_FILENO))
    {
      setlocale (LC_ALL, "");
      bindtextdomain (PACKAGE, LOCALEDIR);
      textdomain (PACKAGE);
    }

  if (argp_parse (&argp, argc, argv, ARGP_IN_ORDER, &idx, nullptr))
    exit (EXIT_FAILURE);
  if (idx != argc)
    {
      if (idx != argc - 1)
	error (EXIT_FAILURE, 0, _("too many arguments"));
      dbgout = fopen (argv[idx], "w");
      if (!dbgout)
	error (EXIT_FAILURE, errno, _("cannot open %s"), argv[idx]);
      dbglev = 1;
    }

  while (!stop && (buf = rmt_read ()))
    {
      switch (buf[0])
	{
	case 'C':
	  close_device ();
	  stop = true;
	  break;

	case 'I':
	  iocop_device (buf + 1);
	  break;

	case 'L':
	  lseek_device (buf + 1);
	  break;

	case 'O':
	  open_device (buf + 1);
	  break;

	case 'R':
	  read_device (buf + 1);
	  break;

	case 'S':
	  status_device (buf + 1);
	  break;

	case 'W':
	  write_device (buf + 1);
	  break;

	default:
	  DEBUG1 (1, "garbage input %s\n", buf);
	  rmt_error_message (EINVAL, N_("Garbage command"));
	  return EXIT_FAILURE;	/* exit status used to be 3 */
	}
    }
  if (device_fd >= 0)
    close_device ();
  free (input_buf_ptr);
  free (record_buffer_ptr);
  return EXIT_SUCCESS;
}
