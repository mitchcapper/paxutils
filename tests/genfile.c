/* Generate a file containing some preset patterns.
   Print statistics for existing files.
   
   Copyright (C) 1995, 1996, 1997, 2001, 2003, 2004, 2005 Free Software
   Foundation, Inc.

   François Pinard <pinard@iro.umontreal.ca>, 1995.
   Sergey Poznyakoff <gray@mirddin.farlep.net>, 2004, 2005.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <system.h>
#include <stdarg.h>
#include <argmatch.h>
#include <argp.h>

#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
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

/* Length of file to generate.  */
static int file_length = 0;

/* Pattern to generate.  */
static enum pattern pattern = DEFAULT_PATTERN;

/* Generate sparse file */
static int sparse_file; 

/* Print stats */
static char *do_stat;

#define DEFAULT_STAT_FORMAT \
  "name,dev,ino,mode,nlink,uid,gid,size,blksize,blocks,atime,mtime,ctime"

/* Size of a block for sparse file */
size_t block_size = 512;

/* Block buffer for sparse file */
char *buffer;

const char *argp_program_version = "genfile (" PACKAGE ") " VERSION;
const char *argp_program_bug_address = "<" PACKAGE_BUGREPORT ">";
static char doc[] = N_("genfile manipulates data files for GNU paxutils test suite.\n"
"\n"
"Modes:\n\n"
"1. Create plain file:\n\n"		       
"  genfile [--file=NAME] [--pattern=PAT] --length=LENGTH\n\n"
"2. Create sparse file:\n\n"
"  genfile --sparse --file=NAME [--block-size=N] \\\n"
"          DISP LETTERS [DISP LETTERS...][DISP]]\n\n"
"3. Print contents of struct stat for each FILE:\n\n"
"  genfile --stat[=FORMAT] FILE [FILE...]\n\n"
"OPTIONS are:\n\v"
"Default FORMAT for --stat is: ") DEFAULT_STAT_FORMAT;

static struct argp_option options[] = {
  {"length", 'l', N_("SIZE"), 0,
   N_("Create file of the given SIZE"), 0 },
  {"file", 'f', N_("NAME"), 0,
   N_("Write to file NAME, instead of standard output"), 0},
  {"pattern", 'p', N_("PATTERN"), 0,
   N_("Fill the file with the given PATTERN. PATTERN is 'default' or 'zeros'"),
   0 },
  {"block-size", 'b', N_("SIZE"), 0,
   N_("Size of a block for sparse file"), 0},
  {"sparse", 's', NULL, 0,
   N_("Generate sparse file. Rest of the command line gives the file map. "
      "It consists of [DISP LETTERS] pairs, each pair describing file "
      "fragment. DISP sets displacement of the next data fragment from the "
      "end of the preceeding one, LETTERS is a string of letters for filling "
      "the fragment. For each letter from LETTERS genfile creates a block of "
      "block-size bytes (see --block-size), filled with that letter and "
      "writes it to the file. If the last argument consists of DISP only, "
      "genfile creates a hole at the end of the file, ending on this "
      "displacement."),
   0 },
  {"stat", 'S', N_("FORMAT"), OPTION_ARG_OPTIONAL,
   N_("Print contents of struct stat for each given file. "
      "FORMAT is a comma-separated list of struct stat fields to be "
      "displayed (you may omit `st_' prefix) and the word `name' for the "
      "file name.  Dates are displayed in UTC as UNIX timestamps, unless "
      "suffixed with `H' (for `human-readable') in which case usual "
      "`tar tv' output format is used."),
   0 },
  { NULL, }
};

static char const * const pattern_args[] = { "default", "zeros", 0 };
static enum pattern const pattern_types[] = {DEFAULT_PATTERN, ZEROS_PATTERN};

static void
die (char const *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "%s: ", program_name);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fprintf (stderr, "\n");
  exit (EXIT_FAILURE);
}

static int
xlat_suffix (off_t *vp, char *p)
{
  if (p[1])
    return 1;
  switch (p[0])
    {
    case 'g':
    case 'G':
      *vp *= 1024;

    case 'm':
    case 'M':
      *vp *= 1024;

    case 'k':
    case 'K':
      *vp *= 1024;
      break;

    default:
      return 1;
    }
  return 0;
}

static off_t
get_size (const char *str, int allow_zero)
{
  char *p;
  off_t n;
  
  n = strtoul (str, &p, 0);
  if (n < 0 || (!allow_zero && n == 0) || (*p && xlat_suffix (&n, p)))
    die (_("Invalid size: %s"), str);
  return n;
}


static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'f':
      file_name = arg;
      break;
      
    case 'l':
      file_length = get_size (arg, 1);
      break;
	  
    case 'p':
      pattern = XARGMATCH ("--pattern", arg, pattern_args, pattern_types);
      break;

    case 'b':
      block_size = get_size (arg, 0);
      break;
      
    case 's':
      sparse_file = 1;
      break; 

    case 'S':
      do_stat = arg ? arg : DEFAULT_STAT_FORMAT;
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
  NULL,
  NULL,
  NULL
};


static void
generate_simple_file (int argc, char **argv)
{
  int i;
  FILE *fp;

  if (argc)
    die (_("too many arguments"));
  
  if (file_name)
    {
      fp = fopen (file_name, "w");
      if (!fp)
	die (_("cannot open `%s'"), file_name);	
    }
  else
    fp = stdout;
  
  switch (pattern)
    {
    case DEFAULT_PATTERN:
      for (i = 0; i < file_length; i++)
	fputc (i & 255, fp);
      break;

    case ZEROS_PATTERN:
      for (i = 0; i < file_length; i++)
	fputc (0, fp);
      break;
    }
  fclose (fp);
}



static void
mkhole (int fd, off_t displ)
{
  if (lseek (fd, displ, SEEK_CUR) == -1)
    {
      perror ("lseek");
      exit (EXIT_FAILURE);
    }
  ftruncate (fd, lseek (fd, 0, SEEK_CUR));
}

static void
mksparse (int fd, off_t displ, char *marks)
{
  for (; *marks; marks++)
    {
      memset (buffer, *marks, block_size);
      if (write (fd, buffer, block_size) != block_size)
	{
	  perror ("write");
	  exit (EXIT_FAILURE);
	}

      if (lseek (fd, displ, SEEK_CUR) == -1)
	{
	  perror ("lseek");
	  exit (EXIT_FAILURE);
	}
    }
}

static void
generate_sparse_file (int argc, char **argv)
{
  int i;
  int fd;

  if (!file_name)
    die (_("cannot generate sparse files on standard output, use --file option"));
  fd = open (file_name, O_CREAT|O_TRUNC|O_RDWR, 0644);
  if (fd < 0)
    die (_("cannot open `%s'"), file_name);

  buffer = malloc (block_size);
  if (!buffer)
    die (_("Not enough memory"));

  for (i = 0; i < argc; i += 2)
    {
      off_t displ = get_size (argv[i], 1);

      if (i == argc-1)
	{
	  mkhole (fd, displ);
	  break;
	}
      else
	mksparse (fd, displ, argv[i+1]);
    }

  close (fd);
}

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
  char *fmt, *p;
  struct stat st;

  if (stat (name, &st))
    {
      perror (name);
      return;
    }

  fmt = strdup (do_stat);
  for (p = strtok (fmt, ","); p; p = strtok (NULL, ","))
    {
      if (memcmp (p, "st_", 3) == 0)
	p += 3;
      if (strcmp (p, "name") == 0)
	printf ("%s", name);
      else if (strcmp (p, "dev") == 0)
	printf ("%lu", (unsigned long) st.st_dev);
      else if (strcmp (p, "ino") == 0)
	printf ("%lu", (unsigned long) st.st_ino);
      else if (strcmp (p, "mode") == 0)
	printf ("%0o", st.st_mode);
      else if (strcmp (p, "nlink") == 0)
	printf ("%lu", (unsigned long) st.st_nlink);
      else if (strcmp (p, "uid") == 0)
	printf ("%ld", (long unsigned) st.st_uid);
      else if (strcmp (p, "gid") == 0)
	printf ("%lu", (unsigned long) st.st_gid);
      else if (strcmp (p, "size") == 0)
	printf ("%lu", (unsigned long) st.st_size);
      else if (strcmp (p, "blksize") == 0)
	printf ("%lu", (unsigned long) st.st_blksize);
      else if (strcmp (p, "blocks") == 0)
	printf ("%lu", (unsigned long) st.st_blocks);
      else if (strcmp (p, "atime") == 0)
	printf ("%lu", (unsigned long) st.st_atime);
      else if (strcmp (p, "atimeH") == 0)
	print_time (st.st_atime);
      else if (strcmp (p, "mtime") == 0)
	printf ("%lu", (unsigned long) st.st_mtime);
      else if (strcmp (p, "mtimeH") == 0)
	print_time (st.st_mtime);
      else if (strcmp (p, "ctime") == 0)
	printf ("%lu", (unsigned long) st.st_ctime);
      else if (strcmp (p, "ctimeH") == 0)
	print_time (st.st_ctime);
      else
	{
	  printf ("\n");
	  fprintf (stderr, _("%s: Unknown field `%s'\n"),
		   program_invocation_short_name, p);
	  exit (EXIT_FAILURE);
	}
      printf (" ");
    }
  printf ("\n");
  free (fmt);
}

int
main (int argc, char **argv)
{
  int index;

  program_name = argv[0];
  setlocale (LC_ALL, "");

  /* Decode command options.  */

  if (argp_parse (&argp, argc, argv, 0, &index, NULL))
    exit (EXIT_FAILURE); 

  argc -= index;
  argv += index;

  if (do_stat)
    {
      if (argc == 0)
	{
	  fprintf (stderr, "%s: --stat requires file names\n",
		   program_invocation_short_name);
	  exit (1);
	}

      while (argc--)
	print_stat (*argv++);
    }
  else if (sparse_file)
    generate_sparse_file (argc, argv);
  else if (file_length == 0)
    argp_help (&argp, stderr, ARGP_HELP_STD_HELP,
	       program_invocation_short_name);
  else
    generate_simple_file (argc, argv);

  exit (EXIT_SUCCESS);
}
