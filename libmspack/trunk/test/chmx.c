#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mspack.h>
#include "md5.h"

#define FILENAME ".test.chmx"

char *error_msg(int error) {
  switch (error) {
  case MSPACK_ERR_OK:         return "no error";
  case MSPACK_ERR_ARGS:       return "bad arguments to library function";
  case MSPACK_ERR_OPEN:       return "error opening file";
  case MSPACK_ERR_READ:       return "read error";
  case MSPACK_ERR_WRITE:      return "write error";
  case MSPACK_ERR_SEEK:       return "seek error";
  case MSPACK_ERR_NOMEMORY:   return "out of memory";
  case MSPACK_ERR_SIGNATURE:  return "bad signature";
  case MSPACK_ERR_DATAFORMAT: return "error in data format";
  case MSPACK_ERR_CHECKSUM:   return "checksum error";
  case MSPACK_ERR_CRUNCH:     return "compression error";
  case MSPACK_ERR_DECRUNCH:   return "decompression error";
  }
  return "unknown error";
}

static int sortfunc(const void *a, const void *b) {
  off_t diff = 
    ((* ((struct mschmd_file **) a))->offset) -
    ((* ((struct mschmd_file **) b))->offset);
  return (diff < 0) ? -1 : ((diff > 0) ? 1 : 0);
}

int main(int argc, char *argv[]) {
  struct mschm_decompressor *chmd;
  struct mschmd_header *chm;
  struct mschmd_file *file, **f;
  unsigned int numf, i;
  FILE *fh;

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  MSPACK_SYS_SELFTEST(i);
  if (i) return 0;

  if ((chmd = mspack_create_chm_decompressor(NULL))) {
    for (argv++; *argv; argv++) {
      printf("%s\n", *argv);
      if ((chm = chmd->open(chmd, *argv))) {

	/* EXTRACT OUT OF ORDER [alpha ordered returned by open()] */
	puts("ALPHA ORDER");
	for (file=chm->files; file; file = file->next) {
	  if (chmd->extract(chmd, file, FILENAME)) {
	    printf("%s: extract error on \"%s\": %s\n",
		   *argv, file->filename, error_msg(chmd->last_error(chmd)));
	  }
	  unlink(FILENAME);
	}
	puts("\nIN ORDER");
	/* EXTRACT IN ORDER [ordered by offset into content section] */
	for (numf=0, file=chm->files; file; file = file->next) numf++;
	if ((f = calloc(numf, sizeof(struct mschmd_file *)))) {
	  for (i=0, file=chm->files; file; file = file->next) f[i++] = file;
	  qsort(f, numf, sizeof(struct mschmd_file *), &sortfunc);
	  for (i = 0; i < numf; i++) {
	    if (chmd->extract(chmd, f[i], FILENAME)) {
	      printf("%s: extract error on \"%s\": %s\n",
		     *argv, f[i]->filename, error_msg(chmd->last_error(chmd)));
	    }
	  }
	  free(f);
	}

	chmd->close(chmd, chm);
      }
      else {
	printf("%s: can't open -- %s\n",
	       *argv, error_msg(chmd->last_error(chmd)));
      }
    }
    mspack_destroy_chm_decompressor(chmd);
  }
  return 0;
}
