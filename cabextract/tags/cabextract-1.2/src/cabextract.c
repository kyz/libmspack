/* cabextract 1.2 - a program to extract Microsoft Cabinet files
 * (C) 2000-2006 Stuart Caie <kyzer@4u.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* cabextract uses libmspack to access cabinet files. libmspack is
 * available from http://www.kyz.uklinux.net/libmspack/
 */

#define _GNU_SOURCE 1

#if HAVE_CONFIG_H
#include <config.h>

#include <stdio.h> /* everyone has this! */

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_CTYPE_H
# include <ctype.h>
#endif

#if HAVE_ERRNO_H
# include <errno.h>
#endif

#if HAVE_FNMATCH_H
# include <fnmatch.h>
#endif

#if HAVE_LIMITS_H
# include <limits.h>
#endif

#if HAVE_STDARG_H
# include <stdarg.h>
#endif

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#if HAVE_STRINGS_H
# include <strings.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_UTIME || HAVE_UTIMES
# if HAVE_UTIME_H
#  include <utime.h>
# else
#  include <sys/utime.h>
# endif
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#if !STDC_HEADERS
# if !HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
# if !HAVE_STRCASECMP
#  define strcasecmp strcmpi
# endif
# if !HAVE_MEMCPY
#  define memcpy(d,s,n) bcopy((s),(d),(n))
# endif
#endif

#ifndef HAVE_MKTIME
extern time_t mktime(struct tm *tp);
#endif

#ifndef FNM_CASEFOLD
# define FNM_CASEFOLD (0)
#endif

#include "getopt.h"

#endif

#include <mspack.h>
#include <md5.h>

/* structures and global variables */
struct option optlist[] = {
  { "directory", 1, NULL, 'd' },
  { "fix",       0, NULL, 'f' },
  { "filter",    1, NULL, 'F' },
  { "help",      0, NULL, 'h' },
  { "list",      0, NULL, 'l' },
  { "lowercase", 0, NULL, 'L' },
  { "pipe",      0, NULL, 'p' },
  { "quiet",     0, NULL, 'q' },
  { "single",    0, NULL, 's' },
  { "test",      0, NULL, 't' },
  { "version",   0, NULL, 'v' },
  { NULL,        0, NULL, 0   }
};

struct file_mem {
  struct file_mem *next;
  dev_t st_dev;
  ino_t st_ino; 
  char *from;
};

struct cabextract_args {
  int help, lower, pipe, view, quiet, single, fix, test;
  char *dir, *filter;
};

/* global variables */
struct mscab_decompressor *cabd = NULL;

struct file_mem *cab_args = NULL;
struct file_mem *cab_exts = NULL;
struct file_mem *cab_seen = NULL;

mode_t user_umask;

struct cabextract_args args = {
  0, 0, 0, 0, 0, 0, 0, 0,
  NULL, NULL
};


/** A special filename. Extracting to this filename will send the output
 * to standard output instead of a file on disk. The magic happens in
 * cabx_open() when the STDOUT_FNAME pointer is given as a filename, so
 * treat this like a constant rather than a string.
 */
char *STDOUT_FNAME = "stdout";

/** A special filename. Extracting to this filename will send the output
 * through an MD5 checksum calculator, instead of a file on disk. The
 * magic happens in cabx_open() when the TEST_FNAME pointer is given as a
 * filename, so treat this like a constant rather than a string. 
 */

char *TEST_FNAME = "test";

/** A global MD5 context, used when a file is written to TEST_FNAME */
struct md5_ctx md5_context;

/** The resultant MD5 checksum, used when a file is written to TEST_FNAME */
unsigned char md5_result[16];

/* prototypes */
static int process_cabinet(char *cabname);

static void load_spanning_cabinets(struct mscabd_cabinet *basecab,
				   char *basename);
static char *find_cabinet_file(char *origcab, char *cabname);
static int unix_path_seperators(struct mscabd_file *files);
static char *create_output_name(unsigned char *fname, unsigned char *dir,
				int lower, int isunix, int unicode);
static void set_date_and_perm(struct mscabd_file *file, char *filename);

static void memorise_file(struct file_mem **fml, char *name, char *from);
static int recall_file(struct file_mem *fml, char *name, char **from);
static void forget_files(struct file_mem **fml);
static int ensure_filepath(char *path);
static char *cab_error(struct mscab_decompressor *cd);

static struct mspack_file *cabx_open(struct mspack_system *this,
				     char *filename, int mode);
static void cabx_close(struct mspack_file *file);
static int cabx_read(struct mspack_file *file, void *buffer, int bytes);
static int cabx_write(struct mspack_file *file, void *buffer, int bytes);
static int cabx_seek(struct mspack_file *file, off_t offset, int mode);
static off_t cabx_tell(struct mspack_file *file);
static void cabx_msg(struct mspack_file *file, char *format, ...);
static void *cabx_alloc(struct mspack_system *this, size_t bytes);
static void cabx_free(void *buffer);
static void cabx_copy(void *src, void *dest, size_t bytes);

/**
 * A cabextract-specific implementation of mspack_system that allows
 * the NULL filename to be opened for writing as a synonym for writing
 * to stdout.
 */
static struct mspack_system cabextract_system = {
  &cabx_open, &cabx_close, &cabx_read,  &cabx_write, &cabx_seek,
  &cabx_tell, &cabx_msg, &cabx_alloc, &cabx_free, &cabx_copy, NULL
};

int main(int argc, char *argv[]) {
  int i, err;

  /* parse options */
  while ((i = getopt_long(argc, argv, "d:fF:hlLpqstv", optlist, NULL)) != -1) {
    switch (i) {
    case 'd': args.dir    = optarg; break;
    case 'f': args.fix    = 1;      break;
    case 'F': args.filter = optarg; break;
    case 'h': args.help   = 1;      break;
    case 'l': args.view   = 1;      break;
    case 'L': args.lower  = 1;      break;
    case 'p': args.pipe   = 1;      break;
    case 'q': args.quiet  = 1;      break;
    case 's': args.single = 1;      break;
    case 't': args.test   = 1;      break;
    case 'v': args.view   = 1;      break;
    }
  }

  if (args.help) {
    fprintf(stderr,
      "Usage: %s [options] [-d dir] <cabinet file(s)>\n\n"
      "This will extract all files from a cabinet or executable cabinet.\n"
      "For multi-part cabinets, only specify the first file in the set.\n\n",
      argv[0]);
    fprintf(stderr,
      "Options:\n"
      "  -v   --version     print version / list cabinet\n"
      "  -h   --help        show this help page\n"
      "  -l   --list        list contents of cabinet\n"
      "  -t   --test        test cabinet integrity\n"
      "  -q   --quiet       only print errors and warnings\n"
      "  -L   --lowercase   make filenames lowercase\n"
      "  -f   --fix         fix (some) corrupted cabinets\n");
    fprintf(stderr,
      "  -p   --pipe        pipe extracted files to stdout\n"
      "  -s   --single      restrict search to cabs on the command line\n"
      "  -F   --filter      extract only files that match the given pattern\n"
      "  -d   --directory   extract all files to the given directory\n\n"
      "cabextract %s (C) 2000-2006 Stuart Caie <kyzer@4u.net>\n"
      "This is free software with ABSOLUTELY NO WARRANTY.\n",
      VERSION);
    return EXIT_FAILURE;
  }

  if (args.test && args.view) {
    fprintf(stderr, "%s: You cannot use --test and --list at the same time.\n"
	    "Try '%s --help' for more information.\n", argv[0], argv[0]);
    return EXIT_FAILURE;
  }

  if (optind == argc) {
    /* no arguments other than the options */
    if (args.view) {
      printf("cabextract version %s\n", VERSION);
      return 0;
    }
    else {
      fprintf(stderr, "%s: No cabinet files specified.\nTry '%s --help' "
	      "for more information.\n", argv[0], argv[0]);
      return EXIT_FAILURE;
    }
  }

  /* memorise command-line cabs if necessary */
  if (args.single) {
    for (i = optind; i < argc; i++) memorise_file(&cab_args, argv[i], NULL);
  }

  /* extracting to stdout implies shutting up on stdout */
  if (args.pipe && !args.view) args.quiet = 1;

  /* open libmspack */
  MSPACK_SYS_SELFTEST(err);
  if (err) {
    if (err == MSPACK_ERR_SEEK) {
      fprintf(stderr,
	      "FATAL ERROR: libmspack is compiled for %d-bit file IO,\n"
	      "             cabextract is compiled for %d-bit file IO.\n",
	      (sizeof(off_t) == 4) ? 64 : 32,
	      (sizeof(off_t) == 4) ? 32 : 64);
    }
    else {
      fprintf(stderr, "FATAL ERROR: libmspack self-test returned %d\n", err);
    }
    return EXIT_FAILURE;
  }

  if (!(cabd = mspack_create_cab_decompressor(&cabextract_system))) {
    fprintf(stderr, "can't create libmspack CAB decompressor\n");
    return EXIT_FAILURE;
  }

  /* obtain user's umask */
  user_umask = umask(0);
  umask(user_umask);

  /* turn on/off 'fix MSZIP' mode */
  cabd->set_param(cabd, MSCABD_PARAM_FIXMSZIP, args.fix);

  /* process cabinets */
  for (i = optind, err = 0; i < argc; i++) {
    err += process_cabinet(argv[i]);
  }

  /* error summary */
  if (!args.quiet) {
    if (err) printf("\nAll done, errors in processing %d file(s)\n", err);
    else printf("\nAll done, no errors.\n");
  }

  /* close libmspack */
  mspack_destroy_cab_decompressor(cabd);

  /* empty file-memory lists */
  forget_files(&cab_args);
  forget_files(&cab_exts);
  forget_files(&cab_seen);

  return err ? EXIT_FAILURE : EXIT_SUCCESS;
}

/**
 * Processes each file argument on the command line, as specified by the
 * command line options. This does the main bulk of work in cabextract.
 *
 * @param basename the file to process
 * @return the number of files with errors, usually 0 for success or 1 for
 *         failure
 */
static int process_cabinet(char *basename) {
  struct mscabd_cabinet *basecab, *cab, *cab2;
  struct mscabd_file *file;
  int isunix, fname_offset, viewhdr = 0;
  char *from, *name;
  int errors = 0;

  /* do not process repeat cabinets */
  if (recall_file(cab_seen, basename, &from) ||
      recall_file(cab_exts, basename, &from)) {
    if (!args.quiet) {
      if (!from) printf("%s: skipping known cabinet\n", basename);
      else printf("%s: skipping known cabinet (from %s)\n", basename, from);
    }
    return 0; /* return success */
  }
  memorise_file(&cab_seen, basename, NULL);

  /* search the file for cabinets */
  if (!(basecab = cabd->search(cabd, basename))) {
    if (cabd->last_error(cabd)) {
      fprintf(stderr, "%s: %s\n", basename, cab_error(cabd));
    }
    else {
      fprintf(stderr, "%s: no valid cabinets found\n", basename);
    }
    return 1;
  }

  /* iterate over all cabinets found in that file */
  for (cab = basecab; cab; cab = cab->next) {

    /* load all spanning cabinets */
    load_spanning_cabinets(cab, basename);

    /* determine whether UNIX or MS-DOS path seperators are used */
    isunix = unix_path_seperators(cab->files);

    /* print headers */
    if (!viewhdr) {
      if (args.view) {
	if (!args.quiet) printf("Viewing cabinet: %s\n", basename);
	printf(" File size | Date       Time     | Name\n");
	printf("-----------+---------------------+-------------\n");
      }
      else {
	if (!args.quiet) {
	  printf("%s cabinet: %s\n", args.test ? "Testing" : "Extracting",
		                     basename);
	}
      }
      viewhdr = 1;
    }

    /* the full UNIX output filename includes the output
     * directory. However, for filtering purposes, we don't want to 
     * include that. So, we work out where the filename part of the 
     * output name begins. This is the same for every extracted file.
     */
    if (args.filter) {
      fname_offset = args.dir ? (strlen(args.dir) + 1) : 0;
    }

    /* process all files */
    for (file = cab->files; file; file = file->next) {
      /* create the full UNIX output filename */
      if (!(name = create_output_name(
	    (unsigned char *) file->filename, (unsigned char *) args.dir,
	    args.lower, isunix, file->attribs & MSCAB_ATTRIB_UTF_NAME)))
      {
	errors++;
	continue;
      }

      /* if filtering, do so now. skip if file doesn't match filter */
      if (args.filter &&
	  fnmatch(args.filter, &name[fname_offset], FNM_CASEFOLD))
      {
	free(name);
	continue;
      }

      /* view, extract or test the file */
      if (args.view) {
	printf("%10u | %02d.%02d.%04d %02d:%02d:%02d | %s\n",
	       file->length, file->date_d, file->date_m, file->date_y,
	       file->time_h, file->time_m, file->time_s, name);
      }
      else if (args.test) {
	if (cabd->extract(cabd, file, TEST_FNAME)) {
	  /* file failed to extract */
	  printf("  %s  failed (%s)\n", name, cab_error(cabd));
	  errors++;
	}
	else {
	  /* file extracted OK, print the MD5 checksum in md5_result. Print
	   * the checksum right-aligned to 79 columns if that's possible,
	   * otherwise just print it 2 spaces after the filename and "OK" */

	  /* "  filename  OK  " is 8 chars + the length of filename,
	   * the MD5 checksum itself is 32 chars. */
	  int spaces = 79 - (strlen(name) + 8 + 32);
	  printf("  %s  OK  ", name);
	  while (spaces-- > 0) putchar(' ');
	  printf("%02x%02x%02x%02x%02x%02x%02x%02x"
		 "%02x%02x%02x%02x%02x%02x%02x%02x\n",
		 md5_result[0], md5_result[1], md5_result[2], md5_result[3],
		 md5_result[4], md5_result[5], md5_result[6], md5_result[7],
		 md5_result[8], md5_result[9], md5_result[10],md5_result[11],
		 md5_result[12],md5_result[13],md5_result[14],md5_result[15]);
	}
      }
      else {
	/* extract the file */
	if (args.pipe) {
	  /* extracting to stdout */
	  if (cabd->extract(cabd, file, STDOUT_FNAME)) {
	    fprintf(stderr, "%s(%s): %s\n", STDOUT_FNAME, name,
		                            cab_error(cabd));
	    errors++;
	  }
	}
	else {
	  /* extracting to a regular file */
	  if (!args.quiet) printf("  extracting %s\n", name);

	  if (!ensure_filepath(name)) {
	    fprintf(stderr, "%s: can't create file path\n", name);
	    errors++;
	  }
	  else {
	    if (cabd->extract(cabd, file, name)) {
	      fprintf(stderr, "%s: %s\n", name, cab_error(cabd));
	      errors++;
	    }
	    else {
	      set_date_and_perm(file, name);
	    }
	  }
	}
      }
      free(name);
    } /* for (all files in cab) */

    /* free the spanning cabinet filenames [not freed by cabd->close()] */
    for (cab2 = cab->prevcab; cab2; cab2 = cab2->prevcab) free(cab2->filename);
    for (cab2 = cab->nextcab; cab2; cab2 = cab2->nextcab) free(cab2->filename);
  } /* for (all cabs) */

  /* free all loaded cabinets */
  cabd->close(cabd, basecab);
  return errors;
}

/**
 * Follows the spanning cabinet chain specified in a cabinet, loading
 * and attaching the spanning cabinets as it goes.
 *
 * @param basecab  the base cabinet to start the chain from.
 * @param basename the full pathname of the base cabinet, so spanning
 *                 cabinets can be found in the same path as the base cabinet.
 * @see find_cabinet_file()
 */
static void load_spanning_cabinets(struct mscabd_cabinet *basecab,
				   char *basename)
{
  struct mscabd_cabinet *cab, *cab2;
  char *name;

  /* load any spanning cabinets -- backwards */
  for (cab = basecab; cab->flags & MSCAB_HDR_PREVCAB; cab = cab->prevcab) {
    if (!(name = find_cabinet_file(basename, cab->prevname))) {
      fprintf(stderr, "%s: can't find %s\n", basename, cab->prevname);
      break;
    }
    if (args.single && !recall_file(cab_args, name, NULL)) break;
    if (!args.quiet) {
      printf("%s: extends backwards to %s (%s)\n", basename,
	     cab->prevname, cab->previnfo);
    }
    if (!(cab2 = cabd->open(cabd,name)) || cabd->prepend(cabd, cab, cab2)) {
      if (cab2) cabd->close(cabd, cab2);
      fprintf(stderr, "%s: %s\n", basename, cab_error(cabd));
      break;
    }
    memorise_file(&cab_exts, name, basename);
  }

  /* load any spanning cabinets -- forwards */
  for (cab = basecab; cab->flags & MSCAB_HDR_NEXTCAB; cab = cab->nextcab) {
    if (!(name = find_cabinet_file(basename, cab->nextname))) {
      fprintf(stderr, "%s: can't find %s\n", basename, cab->nextname);
      break;
    }
    if (args.single && !recall_file(cab_args, name, NULL)) break;
    if (!args.quiet) {
      printf("%s: extends to %s (%s)\n", basename,
	     cab->nextname, cab->nextinfo);
    }
    if (!(cab2 = cabd->open(cabd,name)) || cabd->append(cabd, cab, cab2)) {
      if (cab2) cabd->close(cabd, cab2);
      fprintf(stderr, "%s: %s\n", basename, cab_error(cabd));
      break;
    }
    memorise_file(&cab_exts, name, basename);
  }
}

/**
 * Matches a cabinet's filename case-insensitively in the filesystem and
 * returns the case-correct form.
 *
 * @param origcab if this is non-NULL, the pathname part of this filename
 *                will be extracted, and the search will be conducted in
 *                that directory.
 * @param cabname the internal CAB filename to search for.
 * @return a copy of the full, case-correct filename of the given cabinet
 *         filename, or NULL if the specified filename does not exist on disk.
 */
static char *find_cabinet_file(char *origcab, char *cabname) {
  struct dirent *entry;
  struct stat st_buf;
  int found = 0, len;
  char *tail, *cab;
  DIR *dir;

  /* ensure we have a cabinet name at all */
  if (!cabname || !cabname[0]) return NULL;

  /* find if there's a directory path in the origcab */
  tail = origcab ? strrchr(origcab, '/') : NULL;
  len = (tail - origcab) + 1;

  /* allocate memory for our copy */
  if (!(cab = malloc((tail ? len : 2) + strlen(cabname) + 1))) return NULL;

  /* add the directory path from the original cabinet name, or "." */
  if (tail) memcpy(cab, origcab, (size_t) len);
  else      cab[0]='.', cab[1]='/', len=2;
  cab[len] = '\0';

  /* try accessing the cabinet with its current name (case-sensitive) */
  strcpy(&cab[len], cabname);
  if (stat(cab, &st_buf) == 0) {
    found = 1;
  }
  else {
    /* cabinet was not found, look for it in the current dir */
    cab[len] = '\0';
    if ((dir = opendir(cab))) {
      while ((entry = readdir(dir))) {
	if (strcasecmp(cabname, entry->d_name) == 0) {
	  strcat(cab, entry->d_name);
	  found = (stat(cab, &st_buf) == 0);
	  break;
	}
      }
      closedir(dir);
    }
  }

  if (!found || !S_ISREG(st_buf.st_mode)) {
    /* cabinet not found, or not a regular file */
    free(cab);
    cab = NULL;
  }

  return cab;
}

/**
 * Determines whether UNIX '/' or MS-DOS '\' path seperators are used in
 * the cabinet file. The algorithm is as follows:
 *
 * Look at all slashes in all filenames. If there are no slashes, MS-DOS
 * seperators are assumed (it doesn't matter). If all are backslashes,
 * MS-DOS seperators are assumed. If all are forward slashes, UNIX
 * seperators are assumed.
 *
 * If not all slashes are the same, go through each filename, looking for
 * the first slash.  If the part of the filename up to and including the
 * slash matches the previous filename, that kind of slash is the
 * directory seperator.
 *
 * @param files list of files in the cab file
 * @return 0 for MS-DOS seperators, or 1 for UNIX seperators.
 */
static int unix_path_seperators(struct mscabd_file *files) {
  struct mscabd_file *fi;
  char slash=0, backslash=0, *oldname;
  int oldlen;

  for (fi = files; fi; fi = fi->next) {
    char *p;
    for (p = fi->filename; *p; p++) {
      if (*p == '/') slash = 1;
      if (*p == '\\') backslash = 1;
    }
    if (slash && backslash) break;
  }

  if (slash) {
    /* slashes, but no backslashes = UNIX */
    if (!backslash) return 1;
  }
  else {
    /* no slashes = MS-DOS */
    return 0;
  }

  /* special case if there's only one file - just take the first slash */
  if (!files->next) {
    char c, *p = fi->filename;
    while ((c = *p++)) {
      if (c == '\\') return 0; /* backslash = MS-DOS */
      if (c == '/')  return 1; /* slash = UNIX */
    }
    /* should not happen - at least one slash was found! */
    return 0;
  }

  oldname = NULL;
  oldlen = 0;
  for (fi = files; fi; fi = fi->next) {
    char *name = fi->filename;
    int len = 0;
    while (name[len]) {
      if ((name[len] == '\\') || (name[len] == '/')) break;
      len++;
    }
    if (!name[len]) len = 0; else len++;

    if (len && (len == oldlen)) {
      if (strncmp(name, oldname, (size_t) len) == 0)
	return (name[len-1] == '\\') ? 0 : 1;
    }
    oldname = name;
    oldlen = len;
  }

  /* default */
  return 0;
}

/**
 * Creates a UNIX filename from the internal CAB filename and the given
 * parameters.
 *
 * @param fname  the internal CAB filename.
 * @param dir    a directory path to prepend to the output filename.
 * @param lower  if non-zero, filename should be made lower-case.
 * @param isunix if zero, MS-DOS path seperators are used in the internal
 *               CAB filename. If non-zero, UNIX path seperators are used.
 * @param utf8   if non-zero, the internal CAB filename is encoded in UTF-8.
 * @return a freshly allocated and created filename, or NULL if there was
 *         not enough memory.
 * @see unix_path_seperators()
 */
static char *create_output_name(unsigned char *fname, unsigned char *dir,
			 int lower, int isunix, int utf8)
{
  unsigned char *p, *name, c, *fe, sep, slash;
  unsigned int x;

  sep   = (isunix) ? '/'  : '\\'; /* the path-seperator */
  slash = (isunix) ? '\\' : '/';  /* the other slash */

  /* length of filename */
  x = strlen((char *) fname);
  /* UTF-8 worst case scenario: tolower() expands all chars from 1 to 4 bytes */
  if (utf8) x *= 4;
  /* length of output directory */
  if (dir) x += strlen((char *) dir);
  x += 2;

  if (!(name = malloc(x))) {
    fprintf(stderr, "Can't allocate output filename (%u bytes)\n", x);
    return NULL;
  }
  
  /* start with blank name */
  *name = '\0';

  /* add output directory if needed */
  if (dir) {
    strcpy((char *) name, (char *) dir);
    strcat((char *) name, "/");
  }

  /* remove leading slashes */
  while (*fname == sep) fname++;

  /* copy from fi->filename to new name, converting MS-DOS slashes to UNIX
   * slashes as we go. Also lowercases characters if needed.
   */
  p = &name[strlen((char *)name)];    /* p  = start of output filename */
  fe = &fname[strlen((char *)fname)]; /* fe = end of input filename */

  if (utf8) {
    /* UTF-8 translates unicode characters into 1 to 4 bytes.
     * %00000000000000sssssss -> %0sssssss
     * %0000000000ssssstttttt -> %110sssss %10tttttt
     * %00000ssssttttttuuuuuu -> %1110ssss %10tttttt %10uuuuuu
     * %sssttttttuuuuuuvvvvvv -> %11110sss %10tttttt %10uuuuuu %10vvvvvv
     *
     * Therefore, the inverse is as follows:
     * First char:
     *  0x00 - 0x7F = one byte char
     *  0x80 - 0xBF = invalid
     *  0xC0 - 0xDF = 2 byte char (next char only 0x80-0xBF is valid)
     *  0xE0 - 0xEF = 3 byte char (next 2 chars only 0x80-0xBF is valid)
     *  0xF0 - 0xF7 = 4 byte char (next 3 chars only 0x80-0xBF is valid)
     *  0xF8 - 0xFF = invalid
     */
    do {
      if (fname > fe) {
	fprintf(stderr, "error in UTF-8 decode\n");
	free(name);
	return NULL;	
      }

      /* get next UTF-8 character */
      if ((c = *fname++) < 0x80) x = c;
      else {
	if ((c >= 0xC0) && (c <= 0xDF)) {
	  x = (c & 0x1F) << 6;
	  x |= *fname++ & 0x3F;
	}
	else if ((c >= 0xE0) && (c <= 0xEF)) {
	  x = (c & 0xF) << 12;
	  x |= (*fname++ & 0x3F) << 6;
	  x |= *fname++ & 0x3F;
	}
	else if ((c >= 0xF0) && (c <= 0xF7)) {
          x = (c & 0x7) << 18;
	  x |= (*fname++ & 0x3F) << 12;
	  x |= (*fname++ & 0x3F) << 6;
	  x |= *fname++ & 0x3F;
	}
	else x = '?';
      }

      /* whatever is the path seperator -> '/'
       * whatever is the other slash    -> '\\'
       * otherwise, if lower is set, the lowercase version */
      if      (x == sep)   x = '/';
      else if (x == slash) x = '\\';
      else if (lower)      x = (unsigned int) tolower((int) x);

      /* convert unicode character back to UTF-8 */
      if (x < 0x80) {
	*p++ = (unsigned char) x;
      }
      else if (x < 0x800) {
	*p++ = 0xC0 | (x >> 6);   
	*p++ = 0x80 | (x & 0x3F);
      }
      else if (x < 0x10000) {
	*p++ = 0xE0 | (x >> 12);
	*p++ = 0x80 | ((x >> 6) & 0x3F);
	*p++ = 0x80 | (x & 0x3F);
      }
      else {
        *p++ = 0xF0 | (x >> 18);
	*p++ = 0x80 | ((x >> 12) & 0x3F);
	*p++ = 0x80 | ((x >> 6) & 0x3F);
	*p++ = 0x80 | (x & 0x3F);
      }
    } while (x);
  }
  else {
    /* regular non-utf8 version */
    do {
      c = *fname++;
      if      (c == sep)   c = '/';
      else if (c == slash) c = '\\';
      else if (lower)      c = (unsigned char) tolower((int) c);
    } while ((*p++ = c));
  }

  /* search for "../" in cab filename part and change to "xx/".  This
   * prevents any unintended directory traversal. */
  for (p = &name[dir ? strlen((char *) dir)+1 : 0]; *p; p++) {
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '/')) {
      p[0] = p[1] = 'x';
      p += 2;
    }
  }

  return (char *) name;
}

/**
 * Sets the last-modified time and file permissions on a file.
 *
 * @param file     the internal CAB file whose date, time and attributes will 
 *                 be used.
 * @param filename the name of the UNIX file whose last-modified time and
 *                 file permissions will be set.
 */
static void set_date_and_perm(struct mscabd_file *file, char *filename) {
  mode_t mode;
  struct tm tm;
#if HAVE_UTIME
  struct utimbuf utb;
#elif HAVE_UTIMES
  struct timeval tv[2];
#endif

  /* set last modified date */
  tm.tm_sec   = file->time_s;
  tm.tm_min   = file->time_m;
  tm.tm_hour  = file->time_h;
  tm.tm_mday  = file->date_d;
  tm.tm_mon   = file->date_m - 1;
  tm.tm_year  = file->date_y - 1900;
  tm.tm_isdst = -1;

#if HAVE_UTIME
  utb.actime = utb.modtime = mktime(&tm);
  utime(filename, &utb);
#elif HAVE_UTIMES
  tv[0].tv_sec  = tv[1].tv_sec  = mktime(&tm);
  tv[0].tv_usec = tv[1].tv_usec = 0;
  utimes(filename, &tv[0]);
#endif

  /* set permissions */
  mode = 0444;
  if (  file->attribs & MSCAB_ATTRIB_EXEC)    mode |= 0111;
  if (!(file->attribs & MSCAB_ATTRIB_RDONLY)) mode |= 0222;
  chmod(filename, mode & ~user_umask);
}

/* ------- support functions ------- */

/**
 * Memorises a file by its device and inode number rather than its name. If
 * the file does not exist, it will not be memorised.
 *
 * @param fml  address of the file_mem list that will memorise this file.
 * @param name name of the file to memorise.
 * @param from a string that, if not NULL, will be duplicated stored with
 *             the memorised file.
 * @see recall_file(), forget_files()
 */
static void memorise_file(struct file_mem **fml, char *name, char *from) {
  struct file_mem *fm;
  struct stat st_buf;
  if (stat(name, &st_buf) != 0) return;
  if (!(fm = malloc(sizeof(struct file_mem)))) return;
  fm->st_dev = st_buf.st_dev;
  fm->st_ino = st_buf.st_ino;
  fm->from = (from) ? malloc(strlen(from)+1) : NULL;
  if (fm->from) strcpy(fm->from, from);
  fm->next = *fml;
  *fml = fm;
}

/**
 * Determines if a file has been memorised before, by its device and inode
 * number. If the file does not exist, it cannot be recalled.
 *
 * @param fml  list to search for previously memorised file
 * @param name name of file to recall.
 * @param from if non-NULL, this is an address that the associated "from"
 *             description pointer will be stored.
 * @return non-zero if the file has been previously memorised, zero if the
 *         file is unknown or does not exist.
 * @see memorise_file(), forget_files()
 */
static int recall_file(struct file_mem *fml, char *name, char **from) {
  struct file_mem *fm;
  struct stat st_buf;
  if (stat(name, &st_buf) != 0) return 0;
  for (fm = fml; fm; fm = fm->next) {
    if ((st_buf.st_ino == fm->st_ino) && (st_buf.st_dev == fm->st_dev)) {
      if (from) *from = fm->from; return 1;
    }
  }
  return 0;
}

/**
 * Frees all memory used by a file_mem list.
 *
 * @param fml address of the list to free
 * @see memorise_file()
 */
static void forget_files(struct file_mem **fml) {
  struct file_mem *fm, *next;
  for (fm = *fml; fm; fm = next) {
    next = fm->next;
    free(fm->from);
    free(fm);
  }
  *fml = NULL;
}

/**
 * Ensures that all directory components in a filepath exist. New directory
 * components are created, if necessary.
 *
 * @param path the filepath to check
 * @return non-zero if all directory components in a filepath exist, zero
 *         if components do not exist and cannot be created
 */
static int ensure_filepath(char *path) {
  struct stat st_buf;
  char *p;
  int ok;

  for (p = &path[1]; *p; p++) {
    if (*p != '/') continue;
    *p = '\0';
    ok = (stat(path, &st_buf) == 0) && S_ISDIR(st_buf.st_mode);
    if (!ok) ok = (mkdir(path, 0777 & ~user_umask) == 0);
    *p = '/';
    if (!ok) return 0;
  }
  return 1;
}

/**
 * Returns a string with an error message appropriate for the last error
 * of the CAB decompressor.
 *
 * @param  cd the CAB decompressor.
 * @return a constant string with an appropriate error message.
 */
static char *cab_error(struct mscab_decompressor *cd) {
  switch (cd->last_error(cd)) {
  case MSPACK_ERR_OPEN:
  case MSPACK_ERR_READ:
  case MSPACK_ERR_WRITE:
  case MSPACK_ERR_SEEK:
    return strerror(errno);
  case MSPACK_ERR_NOMEMORY:
    return "out of memory";
  case MSPACK_ERR_SIGNATURE:
    return "bad CAB signature";
  case MSPACK_ERR_DATAFORMAT:
    return "error in CAB data format";
  case MSPACK_ERR_CHECKSUM:
    return "checksum error";
  case MSPACK_ERR_DECRUNCH:
    return "decompression error";
  }
  return "unknown error";
}

struct mspack_file_p {
  FILE *fh;
  char *name, regular_file;
};

static struct mspack_file *cabx_open(struct mspack_system *this,
				    char *filename, int mode)
{
  struct mspack_file_p *fh;
  char *fmode;

  /* Use of the STDOUT_FNAME pointer for a filename means the file should
   * actually be extracted to stdout. Use of the TEST_FNAME pointer for a
   * filename means the file should only be MD5-summed.
   */
  if (filename == STDOUT_FNAME || filename == TEST_FNAME) {
    /* only WRITE mode is valid for these special files */
    if (mode != MSPACK_SYS_OPEN_WRITE) {
      return NULL;
    }
  }

  /* ensure that mode is one of READ, WRITE, UPDATE or APPEND */
  switch (mode) {
  case MSPACK_SYS_OPEN_READ:   fmode = "rb";  break;
  case MSPACK_SYS_OPEN_WRITE:  fmode = "wb";  break;
  case MSPACK_SYS_OPEN_UPDATE: fmode = "r+b"; break;
  case MSPACK_SYS_OPEN_APPEND: fmode = "ab";  break;
  default: return NULL;
  }

  if ((fh = malloc(sizeof(struct mspack_file_p)))) {
    fh->name = filename;

    if (filename == STDOUT_FNAME) {
      fh->regular_file = 0;
      fh->fh = stdout;
      return (struct mspack_file *) fh;
    }
    else if (filename == TEST_FNAME) {
      fh->regular_file = 0;
      fh->fh = NULL;
      md5_init_ctx(&md5_context);
      return (struct mspack_file *) fh;
    }
    else {
      /* regular file - simply attempt to open it */
      fh->regular_file = 1;
      if ((fh->fh = fopen(filename, fmode))) {
	return (struct mspack_file *) fh;
      }
    }
    /* error - free file handle and return NULL */
    free(fh);
  }
  return NULL;
}

static void cabx_close(struct mspack_file *file) {
  struct mspack_file_p *this = (struct mspack_file_p *) file;
  if (this) {
    if (this->name == TEST_FNAME) {
      md5_finish_ctx(&md5_context, (void *) &md5_result);
    }
    else if (this->regular_file) {
      fclose(this->fh);
    }
    free(this);
  }
}

static int cabx_read(struct mspack_file *file, void *buffer, int bytes) {
  struct mspack_file_p *this = (struct mspack_file_p *) file;
  if (this && this->regular_file) {
    size_t count = fread(buffer, 1, (size_t) bytes, this->fh);
    if (!ferror(this->fh)) return (int) count;
  }
  return -1;
}

static int cabx_write(struct mspack_file *file, void *buffer, int bytes) {
  struct mspack_file_p *this = (struct mspack_file_p *) file;
  if (this) {
    if (this->name == TEST_FNAME) {
      md5_process_bytes(buffer, (size_t) bytes, &md5_context);
      return bytes;
    }
    else {
      /* regular files and the stdout writer */
      size_t count = fwrite(buffer, 1, (size_t) bytes, this->fh);
      if (!ferror(this->fh)) return (int) count;
    }
  }
  return -1;
}

static int cabx_seek(struct mspack_file *file, off_t offset, int mode) {
  struct mspack_file_p *this = (struct mspack_file_p *) file;
  if (this && this->regular_file) {
    switch (mode) {
    case MSPACK_SYS_SEEK_START: mode = SEEK_SET; break;
    case MSPACK_SYS_SEEK_CUR:   mode = SEEK_CUR; break;
    case MSPACK_SYS_SEEK_END:   mode = SEEK_END; break;
    default: return -1;
    }
#if HAVE_FSEEKO
    return fseeko(this->fh, offset, mode);
#else
    return fseek(this->fh, offset, mode);
#endif
  }
  return -1;
}

static off_t cabx_tell(struct mspack_file *file) {
  struct mspack_file_p *this = (struct mspack_file_p *) file;
#if HAVE_FSEEKO
  return (this && this->regular_file) ? (off_t) ftello(this->fh) : 0;
#else
  return (this && this->regular_file) ? (off_t) ftell(this->fh) : 0;
#endif
}

static void cabx_msg(struct mspack_file *file, char *format, ...) {
  va_list ap;
  if (file) {
    fprintf(stderr, "%s: ", ((struct mspack_file_p *) file)->name);
  }
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fputc((int) '\n', stderr);
  fflush(stderr);
}
static void *cabx_alloc(struct mspack_system *this, size_t bytes) {
  return malloc(bytes);
}
static void cabx_free(void *buffer) {
  free(buffer);
}
static void cabx_copy(void *src, void *dest, size_t bytes) {
  memcpy(dest, src, bytes);
}
