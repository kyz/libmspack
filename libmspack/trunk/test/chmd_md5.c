#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mspack.h>
#include "md5.h"
#include "error.h"

#define FILENAME ".test.chmd"

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
	for (file=chm->files; file; file = file->next) {
	  if (chmd->extract(chmd, file, FILENAME)) {
	    fprintf(stderr, "%s: extract error on \"%s\": %s\n",
		    *argv, file->filename, ERROR(chmd));
	    exit(1);
	  }
	  if ((fh = fopen(FILENAME, "rb"))) {
	    unsigned char buf[16];
	    memset(buf, 0, 16);
	    md5_stream (fh, &buf[0]);
	    fclose(fh);
	    printf("%02x%02x%02x%02x%02x%02x%02x%02x"
		   "%02x%02x%02x%02x%02x%02x%02x%02x %s\n",
		   buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6],
		   buf[7], buf[8], buf[9], buf[10], buf[11], buf[12],
		   buf[13], buf[14], buf[15], file->filename);
	  }
	  unlink(FILENAME);
	}
	/* EXTRACT IN ORDER [ordered by offset into content section] */
	for (numf=0, file=chm->files; file; file = file->next) numf++;
	if ((f = calloc(numf, sizeof(struct mschmd_file *)))) {
	  for (i=0, file=chm->files; file; file = file->next) f[i++] = file;
	  qsort(f, numf, sizeof(struct mschmd_file *), &sortfunc);
	  for (i = 0; i < numf; i++) {
	    if (chmd->extract(chmd, f[i], ".test")) {
	      fprintf(stderr, "%s: extract error on \"%s\": %s\n",
		      *argv, f[i]->filename, ERROR(chmd));
	      exit(1);
	    }
	  }
	  free(f);
	}

	chmd->close(chmd, chm);
      }
      else {
	fprintf(stderr, "%s: can't open -- %s\n", *argv, ERROR(chmd));
      }
    }
    mspack_destroy_chm_decompressor(chmd);
  }
  else {
    fprintf(stderr, "%s: can't make CHM decompressor\n", *argv);
  }
  return 0;
}
