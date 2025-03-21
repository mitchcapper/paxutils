/* Multi-purpose tool for tar and cpio testsuite.

   Copyright (C) 1995-1997, 2001-2018, 2023-2024 Free Software Foundation, Inc.

   François Pinard <pinard@iro.umontreal.ca>, 1995.
   Sergey Poznyakoff <gray@gnu.org>, 2004-2018.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <system.h>
#include <signal.h>
#include <stdarg.h>
#include <argmatch.h>
#include <argp.h>
#include <argcv.h>
#include <parse-datetime.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <c-ctype.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>

#define EXIT_USAGE 2
#define EXIT_UNAVAILABLE 3

#if ! defined SIGCHLD && defined SIGCLD
# define SIGCHLD SIGCLD
#endif

enum pattern
{
  DEFAULT_PATTERN,
  ZEROS_PATTERN
};

/* The name this program was run with. */
const char *program_name;

/* Name of file to generate */
static char *file_name;

/* Name of the file-list file: */
static char *files_from;
static char filename_terminator = '\n';

/* Length of file to generate.  */
static off_t file_length = 0;
static off_t seek_offset = 0;

/* Pattern to generate.  */
static enum pattern pattern = DEFAULT_PATTERN;

/* Next checkpoint number */
intmax_t checkpoint;

enum genfile_mode
  {
    mode_generate,
    mode_sparse,
    mode_stat,
    mode_exec,
    mode_set_times
  };

enum genfile_mode mode = mode_generate;

#define DEFAULT_STAT_FORMAT \
  "name,dev,ino,mode,nlink,uid,gid,size,blksize,blocks,atime,mtime,ctime"

/* Format for --stat option */
static char *stat_format = DEFAULT_STAT_FORMAT;

/* Size of a block for sparse file */
idx_t block_size = 512;

/* Block buffer for sparse file */
char *buffer;

/* Checkpoint granularity option for mode == mode_exec */
char *checkpoint_granularity;

/* Time for --touch option */
struct timespec touch_time;

/* Verbose mode */
bool verbose;

/* Quiet mode */
bool quiet;

/* Don't dereference symlinks (for --stat) */
bool no_dereference_option;

const char *argp_program_version = "genfile (" PACKAGE ") " VERSION;
const char *argp_program_bug_address = "<" PACKAGE_BUGREPORT ">";
static char doc[] = N_("genfile manipulates data files for GNU paxutils test suite.\n"
"OPTIONS are:\n");

#define OPT_CHECKPOINT 256
#define OPT_TOUCH      257
#define OPT_APPEND     258
#define OPT_TRUNCATE   259
#define OPT_EXEC       260
#define OPT_DATE       261
#define OPT_VERBOSE    262
#define OPT_SEEK       263
#define OPT_DELETE     264

static struct argp_option options[] = {
#define GRP 0
  {nullptr, 0, nullptr, 0,
   N_("File creation options:"), GRP},
  {"length", 'l', N_("SIZE"), 0,
   N_("Create file of the given SIZE"), GRP+1 },
  {"file", 'f', N_("NAME"), 0,
   N_("Write to file NAME, instead of standard output"), GRP+1},
  {"files-from", 'T', N_("FILE"), 0,
   N_("Read file names from FILE"), GRP+1},
  {"null", '0', nullptr, 0,
   N_("-T reads null-terminated names"), GRP+1},
  {"pattern", 'p', N_("PATTERN"), 0,
   N_("Fill the file with the given PATTERN. PATTERN is 'default' or 'zeros'"),
   GRP+1 },
  {"block-size", 'b', N_("SIZE"), 0,
   N_("Size of a block for sparse file"), GRP+1},
  {"sparse", 's', nullptr, 0,
   N_("Generate sparse file. Rest of the command line gives the file map."),
   GRP+1 },
  {"seek", OPT_SEEK, N_("OFFSET"), 0,
   N_("Seek to the given offset before writing data"),
   GRP+1 },
  {"quiet", 'q', nullptr, 0,
   N_("Suppress non-fatal diagnostic messages") },
#undef GRP
#define GRP 10
  {nullptr, 0, nullptr, 0,
   N_("File statistics options:"), GRP},

  {"stat", 'S', N_("FORMAT"), OPTION_ARG_OPTIONAL,
   N_("Print contents of struct stat for each given file. Default FORMAT is: ")
   DEFAULT_STAT_FORMAT,
   GRP+1 },
  {"no-dereference", 'h', nullptr, 0,
   N_("stat symbolic links instead of referenced files"),
   GRP+1 },

  {"set-times", 't', nullptr, 0,
   N_("Set access and modification times of the files to the time supplied"
      " by --date option"),
   GRP+1 },

#undef GRP
#define GRP 20
  {nullptr, 0, nullptr, 0,
   N_("Synchronous execution options:"), GRP},

  {"run", 'r', N_("N"), OPTION_ARG_OPTIONAL,
   N_("Execute ARGS. Trigger checkpoints every Nth record (default 1). Useful with --checkpoint and one of --cut, --append, --touch, --unlink"),
   GRP+1 },
  {"checkpoint", OPT_CHECKPOINT, N_("NUMBER"), 0,
   N_("Perform given action (see below) upon reaching checkpoint NUMBER"),
   GRP+1 },
  {"date", OPT_DATE, N_("STRING"), 0,
   N_("Set date for next --touch option"),
   GRP+1 },
  {"verbose", OPT_VERBOSE, nullptr, 0,
   N_("Display executed checkpoints and exit status of COMMAND"),
   GRP+1 },
#undef GRP
#define GRP 30
  {nullptr, 0, nullptr, 0,
   N_("Synchronous execution actions. These are executed when checkpoint number given by --checkpoint option is reached."), GRP},

  {"cut", OPT_TRUNCATE, N_("FILE"), 0,
   N_("Truncate FILE to the size specified by previous --length option (or 0, if it is not given)"),
   GRP+1 },
  {"truncate", 0, nullptr, OPTION_ALIAS, nullptr, GRP+1 },
  {"append", OPT_APPEND, N_("FILE"), 0,
   N_("Append SIZE bytes to FILE. SIZE is given by previous --length option."),
   GRP+1 },
  {"touch", OPT_TOUCH, N_("FILE"), 0,
   N_("Update the access and modification times of FILE"),
   GRP+1 },
  {"exec", OPT_EXEC, N_("COMMAND"), 0,
   N_("Execute COMMAND"),
   GRP+1 },
  {"delete", OPT_DELETE, N_("FILE"), 0,
   N_("Delete FILE"),
   GRP+1 },
  {"unlink", 0, 0, OPTION_ALIAS, nullptr, GRP+1},
#undef GRP
  { nullptr, }
};

static char const * const pattern_args[] = { "default", "zeros", nullptr };
static enum pattern const pattern_types[] = {DEFAULT_PATTERN, ZEROS_PATTERN};

/* Multiply *VP by the base denoted by P.
   Return -1 if invalid, 0 if OK, 1 if overflow.  */
static int
xlat_suffix (off_t *vp, const char *p)
{
  if (!p[0])
    return 0;

  if (!p[1])
    switch (p[0])
      {
      case 'k': case 'K': return ckd_mul (vp, *vp, 1 << 10);
      case 'm': case 'M': return ckd_mul (vp, *vp, 1 << 20);
      case 'g': case 'G': return ckd_mul (vp, *vp, 1 << 30);
      }

  return -1;
}

static off_t
get_size (const char *str)
{
  char *p;
  errno = 0;
  intmax_t s = strtoimax (str, &p, 10);
  if (p == str || (errno && errno != ERANGE))
    error (EXIT_USAGE, 0, _("Invalid size: %s"), str);
  if (s < 0)
    error (EXIT_USAGE, 0, _("Negative size: %s"), str);
  off_t v;
  if (errno || ckd_add (&v, s, 0))
    error (EXIT_USAGE, 0, _("Number out of allowed range: %s"), str);
  int bad_suffix = xlat_suffix (&v, p);
  if (bad_suffix < 0)
    error (EXIT_USAGE, 0, _("Invalid size: %s"), str);
  if (bad_suffix)
    error (EXIT_USAGE, 0, _("Number out of allowed range: %s"), str);
  return v;
}

void
verify_file (char *file_name)
{
  if (file_name)
    {
      struct stat st;

      if (stat (file_name, &st))
	error (0, errno, _("stat(%s) failed"), file_name);

      if (st.st_size < 0 || st.st_size - seek_offset != file_length)
	{
	  intmax_t requested = st.st_size, actual = file_length;
	  error (EXIT_FAILURE, 0, _("requested file length %jd, actual %jd"),
		 requested, actual);
	}

      if (!quiet && mode == mode_sparse && !ST_IS_SPARSE (st))
	error (EXIT_UNAVAILABLE, 0, _("created file is not sparse"));
    }
}

struct action
{
  struct action *next;
  intmax_t checkpoint;
  int action;
  char *name;
  off_t size;
  enum pattern pattern;
  struct timespec ts;
};

static struct action *action_head, *action_tail;

void
reg_action (int action, char *arg)
{
  struct action *act = ximalloc (sizeof *act);
  act->checkpoint = checkpoint;
  act->action = action;
  act->pattern = pattern;
  act->ts = touch_time;
  act->size = file_length;
  act->name = arg;
  act->next = nullptr;
  if (action_tail)
    action_tail->next = act;
  else
    action_head = act;
  action_tail = act;
}

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case '0':
      filename_terminator = 0;
      break;

    case 'f':
      file_name = arg;
      break;

    case 'l':
      file_length = get_size (arg);
      break;

    case 'p':
      pattern = XARGMATCH ("--pattern", arg, pattern_args, pattern_types);
      break;

    case 'b':
      {
	off_t s = get_size (arg);
	if (ckd_add (&block_size, s, 0))
	  error (EXIT_USAGE, 0, _("Number out of allowed range: %s"), arg);
      }
      break;

    case 'q':
      quiet = true;
      break;

    case 's':
      mode = mode_sparse;
      break;

    case 'S':
      mode = mode_stat;
      if (arg)
	stat_format = arg;
      break;

    case 't':
      mode = mode_set_times;
      break;

    case 'h':
      no_dereference_option = true;
      break;

    case 'r':
      {
	mode = mode_exec;
	if (!arg)
	  arg = "1";
	static char const opt[] = "--checkpoint=";
	idx_t arglen = strlen (arg);
	checkpoint_granularity = ximalloc (sizeof opt + arglen);
	char *p = mempcpy (checkpoint_granularity, opt, sizeof opt - 1);
	memcpy (p, arg, arglen + 1);
      }
      break;

    case 'T':
      files_from = arg;
      break;

    case OPT_SEEK:
      seek_offset = get_size (arg);
      break;

    case OPT_CHECKPOINT:
      {
	char *p;
	checkpoint = strtoimax (arg, &p, 0);
	if (checkpoint < 0 || *p)
	  argp_error (state, _("Error parsing number near `%s'"), p);
      }
      break;

    case OPT_DATE:
      if (! parse_datetime (&touch_time, arg, nullptr))
	argp_error (state, _("Unknown date format"));
      break;

    case OPT_APPEND:
    case OPT_TRUNCATE:
    case OPT_TOUCH:
    case OPT_EXEC:
    case OPT_DELETE:
      reg_action (key, arg);
      break;

    case OPT_VERBOSE:
      verbose = true;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp argp = {
  options,
  parse_opt,
  N_("[ARGS...]"),
  doc,
  nullptr,
  nullptr,
  nullptr
};


void
fill (FILE *fp, off_t length, enum pattern pattern)
{
  switch (pattern)
    {
    case DEFAULT_PATTERN:
      for (off_t i = 0; i < length; i++)
	fputc (i & 255, fp);
      break;

    case ZEROS_PATTERN:
      for (off_t i = 0; i < length; i++)
	fputc (0, fp);
      break;
    }
}

/* Generate Mode: usual files */
static void
generate_simple_file (char *filename)
{
  FILE *fp;

  if (filename)
    {
      fp = fopen (filename, seek_offset ? "rb+" : "wb");
      if (!fp)
	error (EXIT_FAILURE, errno, _("cannot open `%s'"), filename);
    }
  else
    fp = stdout;

  if (fseeko (fp, seek_offset, SEEK_SET) < 0)
    error (EXIT_FAILURE, errno, "%s", _("cannot seek"));

  fill (fp, file_length, pattern);

  fclose (fp);
}

/* A simplified version of the same function from tar */
static bool
read_name_from_file (FILE *fp, struct obstack *stk)
{
  int c;
  idx_t counter = 0;

  while (true)
    {
      c = getc (fp);
      if (c < 0 || c == filename_terminator)
	break;
      if (c == 0)
	error (EXIT_FAILURE, 0, _("file name contains null character"));
      obstack_1grow (stk, c);
      counter++;
    }

  obstack_1grow (stk, 0);

  return c < 0 && counter == 0;
}

void
generate_files_from_list ()
{
  FILE *fp = strcmp (files_from, "-") ? fopen (files_from, "rb") : stdin;
  struct obstack stk;

  if (!fp)
    error (EXIT_FAILURE, errno, _("cannot open `%s'"), files_from);

  obstack_init (&stk);
  while (!read_name_from_file (fp, &stk))
    {
      char *name = obstack_finish (&stk);
      generate_simple_file (name);
      verify_file (name);
      obstack_free (&stk, name);
    }
  fclose (fp);
  obstack_free (&stk, nullptr);
}


/* Generate Mode: sparse files */

static void
mkhole (int fd, off_t displ)
{
  off_t offset = lseek (fd, displ, SEEK_CUR);
  if (offset < 0)
    error (EXIT_FAILURE, errno, "lseek");
  if (ftruncate (fd, offset) < 0)
    error (EXIT_FAILURE, errno, "ftruncate");
}

static void
mksparse (int fd, off_t displ, char *marks)
{
  if (lseek (fd, displ, SEEK_CUR) < 0)
    error (EXIT_FAILURE, errno, "lseek");

  for (; *marks; marks++)
    {
      memset (buffer, *marks, block_size);
      ssize_t written = write (fd, buffer, block_size);
      if (written < 0)
	error (EXIT_FAILURE, errno, "write");
      if (written != block_size)
	error (EXIT_FAILURE, 0, "write");
    }
}

static bool
make_fragment (int fd, char *offstr, char *mapstr)
{
  off_t displ = get_size (offstr);

  if (ckd_add (&file_length, file_length, displ))
    error (EXIT_USAGE, 0, _("Number out of allowed range: %s"), offstr);

  if (!mapstr || !*mapstr)
    {
      mkhole (fd, displ);
      return true;
    }
  else if (*mapstr == '=')
    {
      off_t n = get_size (mapstr + 1);

      switch (pattern)
	{
	case DEFAULT_PATTERN:
	  for (idx_t i = 0; i < block_size; i++)
	    buffer[i] = i & 255;
	  break;

	case ZEROS_PATTERN:
	  memset (buffer, 0, block_size);
	  break;
	}

      if (lseek (fd, displ, SEEK_CUR) < 0)
	error (EXIT_FAILURE, errno, "lseek");

      for (; n; n--)
	{
	  ssize_t written = write (fd, buffer, block_size);
	  if (written < 0)
	    error (EXIT_FAILURE, errno, "write");
	  if (written != block_size)
	    error (EXIT_FAILURE, 0, "write");
	  file_length += block_size;
	}
    }
  else
    {
      mksparse (fd, displ, mapstr);
      off_t mapstrlen = strlen (mapstr);
      file_length += block_size * mapstrlen;
    }
  return false;
}

static void
generate_sparse_file (int argc, char **argv)
{
  int fd;
  int flags = O_CREAT | O_RDWR | O_BINARY;

  if (!file_name)
    error (EXIT_USAGE, 0,
	   _("cannot generate sparse files on standard output, use --file option"));
  if (!seek_offset)
    flags |= O_TRUNC;
  fd = open (file_name, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0)
    error (EXIT_FAILURE, errno, _("cannot open `%s'"), file_name);

  buffer = ximalloc (block_size);

  file_length = 0;

  while (argc)
    {
      if (argv[0][0] == '-' && !argv[0][1])
	{
	  char buf[256];
	  while (fgets (buf, sizeof (buf), stdin))
	    {
	      idx_t n = strlen (buf);

	      while (n > 0 && c_isspace (buf[n - 1]))
		buf[--n] = 0;

	      n = strcspn (buf, " \t");
	      buf[n++] = 0;
	      while (buf[n] && c_isblank (buf[n]))
		++n;
	      make_fragment (fd, buf, buf + n);
	    }
	  ++argv;
	  --argc;
	}
      else
	{
	  if (make_fragment (fd, argv[0], argv[1]))
	    break;
	  argc -= 2;
	  argv += 2;
	}
    }

  close (fd);
}


/* Status Mode */

#define PRINT_INT(expr) \
  do \
    { \
      if (EXPR_SIGNED (expr)) \
	{ \
	  intmax_t printval = expr; \
	  printf ("%jd", printval); \
	} \
      else \
	{ \
	  uintmax_t printval = expr; \
	  printf ("%ju", printval); \
	} \
    } \
  while (false)

void
print_time (time_t t)
{
  char buf[20]; /* ccyy-mm-dd HH:MM:SS\0 */
  strftime (buf, sizeof buf, "%Y-%m-%d %H:%M:%S", gmtime (&t));
  printf ("%s ", buf);
}

void
print_stat (const char *name)
{
  struct stat st;

  if ((no_dereference_option ? lstat : stat) (name, &st))
    {
      error (0, errno, _("stat(%s) failed"), name);
      return;
    }

  char *fmt = strdup (stat_format);
  for (char *p = strtok (fmt, ","); p; )
    {
      if (strncmp (p, "st_", 3) == 0)
	p += 3;
      if (strcmp (p, "name") == 0)
	printf ("%s", name);
      else if (strcmp (p, "dev") == 0)
	PRINT_INT (st.st_dev);
      else if (strcmp (p, "ino") == 0)
	PRINT_INT (st.st_ino);
      else if (strncmp (p, "mode", 4) == 0)
	{
	  uintmax_t val = st.st_mode;

	  if (c_ispunct (p[4]))
	    {
	      char *q;

	      val &= strtoumax (p + 5, &q, 8);
	      if (*q)
		{
		  printf ("\n");
		  error (EXIT_USAGE, 0, _("incorrect mask (near `%s')"), q);
		}
	    }
	  else if (p[4])
	    {
	      printf ("\n");
	      error (EXIT_USAGE, 0, _("Unknown field `%s'"), p);
	    }
	  printf ("%0jo", val);
	}
      else if (strcmp (p, "nlink") == 0)
	PRINT_INT (st.st_nlink);
      else if (strcmp (p, "uid") == 0)
	PRINT_INT (st.st_uid);
      else if (strcmp (p, "gid") == 0)
	PRINT_INT (st.st_gid);
      else if (strcmp (p, "size") == 0)
	PRINT_INT (st.st_size);
      else if (strcmp (p, "blksize") == 0)
	PRINT_INT (st.st_blksize);
      else if (strcmp (p, "blocks") == 0)
	PRINT_INT (st.st_blocks);
      else if (strcmp (p, "atime") == 0)
	PRINT_INT (st.st_atime);
      else if (strcmp (p, "atimeH") == 0)
	print_time (st.st_atime);
      else if (strcmp (p, "mtime") == 0)
	PRINT_INT (st.st_mtime);
      else if (strcmp (p, "mtimeH") == 0)
	print_time (st.st_mtime);
      else if (strcmp (p, "ctime") == 0)
	PRINT_INT (st.st_ctime);
      else if (strcmp (p, "ctimeH") == 0)
	print_time (st.st_ctime);
      else if (strcmp (p, "sparse") == 0)
	printf ("%d", ST_IS_SPARSE (st));
      else
	{
	  printf ("\n");
	  error (EXIT_USAGE, 0, _("Unknown field `%s'"), p);
	}
      p = strtok (nullptr, ",");
      if (p)
	printf (" ");
    }
  printf ("\n");
  free (fmt);
}

void
set_times (char const *name)
{
  struct timespec ts[2];

  ts[0] = ts[1] = touch_time;
  if (utimensat (AT_FDCWD, name, ts,
		 no_dereference_option ? AT_SYMLINK_NOFOLLOW : 0)
      < 0)
    {
      error (EXIT_FAILURE, errno, _("cannot set time on `%s'"), name);
    }
}

/* Exec Mode */

void
exec_checkpoint (struct action *p)
{
  if (verbose)
    printf ("processing checkpoint %jd\n", p->checkpoint);
  switch (p->action)
    {
    case OPT_TOUCH:
      {
	struct timespec ts[2];

	ts[0] = ts[1] = p->ts;
	if (utimensat (AT_FDCWD, p->name, ts,
		       no_dereference_option ? AT_SYMLINK_NOFOLLOW : 0)
	    < 0)
	  {
	    error (0, errno, _("cannot set time on `%s'"), p->name);
	    break;
	  }
      }
      break;

    case OPT_APPEND:
      {
	FILE *fp = fopen (p->name, "ab");
	if (!fp)
	  {
	    error (0, errno, _("cannot open `%s'"), p->name);
	    break;
	  }

	fill (fp, p->size, p->pattern);
	fclose (fp);
      }
      break;

    case OPT_TRUNCATE:
      {
	int fd = open (p->name, O_RDWR | O_BINARY);
	if (fd < 0)
	  {
	    error (0, errno, _("cannot open `%s'"), p->name);
	    break;
	  }
	if (ftruncate (fd, p->size) < 0)
	  {
	    error (0, errno, _("cannot truncate `%s'"), p->name);
	    break;
	  }
	close (fd);
      }
      break;

    case OPT_EXEC:
      if (system (p->name) != 0)
	error (0, 0, _("command failed: %s"), p->name);
      break;

    case OPT_DELETE:
      {
	struct stat st;
	if (stat (p->name, &st))
	  error (0, errno, _("cannot stat `%s'"), p->name);
	else if (S_ISDIR (st.st_mode))
	  {
	    if (rmdir (p->name))
	      error (0, errno, _("cannot remove directory `%s'"), p->name);
	  }
	else if (unlink (p->name))
	  error (0, errno, _("cannot unlink `%s'"), p->name);
      }
      break;

    default:
      abort ();
    }
}

void
process_checkpoint (intmax_t n)
{
  struct action *prev = nullptr;

  for (struct action *p = action_head; p; )
    {
      struct action *next = p->next;

      if (p->checkpoint <= n)
	{
	  exec_checkpoint (p);
	  /* Remove the item from the list */
	  if (prev)
	    prev->next = next;
	  else
	    action_head = next;
	  if (!next)
	    action_tail = prev;
	  free (p);
	}
      else
	prev = p;

      p = next;
    }
}

#define CHECKPOINT_TEXT "genfile checkpoint"

void
exec_command (int argc, char **argv)
{
  /* Insert --checkpoint option.
     FIXME: This assumes that argv does not use traditional tar options
     (without dash).
  */
  int xargc = argc + 5;
  char **xargv = xinmalloc (xargc + 1, sizeof *xargv);
  xargv[0] = argv[0];
  xargv[1] = checkpoint_granularity;
  xargv[2] = "--checkpoint-action";
  xargv[3] = "echo=" CHECKPOINT_TEXT " %u";
  xargv[4] = "--checkpoint-action";
  xargv[5] = "wait=SIGUSR1";
  memcpy (&xargv[6], &argv[1], argc * sizeof *xargv);

#ifdef SIGCHLD
  /* System V fork+wait does not work if SIGCHLD is ignored.  */
  signal (SIGCHLD, SIG_DFL);
#endif

  int fd[2];
  if (pipe (fd) < 0)
    error (EXIT_FAILURE, errno, "pipe");

  pid_t pid = fork ();
  if (pid < 0)
    error (EXIT_FAILURE, errno, "fork");

  if (pid == 0)
    {
      /* Child */

      /* Pipe stderr */
      dup2 (fd[1], STDERR_FILENO);
      close (fd[0]);

      /* Make sure POSIX locale is used */
      setenv ("LC_ALL", "POSIX", 1);

      execvp (xargv[0], xargv);
      error (EXIT_FAILURE, errno, "execvp %s", xargv[0]);
    }

  /* Master */
  close (fd[1]);
  FILE *fp = fdopen (fd[0], "rb");
  if (!fp)
    error (EXIT_FAILURE, errno, "fdopen");

  while (true)
    {
      char buf[128];
      char *p = fgets (buf, sizeof buf, fp);
      if (!p)
	break;
      while (*p && !c_isspace (*p) && *p != ':')
	p++;

      if (*p == ':')
	{
	  for (p++; *p && c_isspace (*p); p++)
	    ;

	  if (*p
	      && strncmp (p, CHECKPOINT_TEXT, sizeof CHECKPOINT_TEXT - 1) == 0)
	    {
	      char *end;
	      intmax_t n = strtoimax (p + sizeof CHECKPOINT_TEXT - 1,
				      &end, 10);
	      if (!(*end && !c_isspace (*end)))
		{
		  process_checkpoint (n);
		  kill (pid, SIGUSR1);
		  continue;
		}
	    }
	}
      fprintf (stderr, "%s", buf);
    }

  /* Collect exit status */
  int status;
  waitpid (pid, &status, 0);

  if (verbose)
    {
      if (WIFEXITED (status))
	{
	  if (WEXITSTATUS (status) == 0)
	    printf (_("Command exited successfully\n"));
	  else
	    printf (_("Command failed with status %d\n"),
		    WEXITSTATUS (status));
	}
      else if (WIFSIGNALED (status))
	printf (_("Command terminated on signal %d\n"), WTERMSIG (status));
      else if (WIFSTOPPED (status))
	printf (_("Command stopped on signal %d\n"), WSTOPSIG (status));
#ifdef WCOREDUMP
      else if (WCOREDUMP (status))
	printf (_("Command dumped core\n"));
#endif
      else
	printf(_("Command terminated\n"));
    }

  if (WIFEXITED (status))
    exit (WEXITSTATUS (status));
  exit (EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
  int index;

  program_name = argv[0];
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  parse_datetime (&touch_time, "now", nullptr);

  /* Decode command options.  */

  if (argp_parse (&argp, argc, argv, 0, &index, nullptr))
    exit (EXIT_USAGE);

  argc -= index;
  argv += index;

  switch (mode)
    {
    case mode_stat:
      if (argc == 0)
	error (EXIT_USAGE, 0, _("--stat requires file names"));

      while (argc--)
	print_stat (*argv++);
      break;

    case mode_set_times:
      if (argc == 0)
	error (EXIT_USAGE, 0, _("--set-times requires file names"));

      while (argc--)
	set_times (*argv++);
      break;

    case mode_sparse:
      generate_sparse_file (argc, argv);
      verify_file (file_name);
      break;

    case mode_generate:
      if (argc)
	error (EXIT_USAGE, 0, _("too many arguments"));
      if (files_from)
	generate_files_from_list ();
      else
	{
	  generate_simple_file (file_name);
	  verify_file (file_name);
	}
      break;

    case mode_exec:
      exec_command (argc, argv);
      break;

    default:
      /* Just in case */
      abort ();
    }
  exit (EXIT_SUCCESS);
}
