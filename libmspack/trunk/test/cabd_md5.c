#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mspack.h>
#include "md5.h"

#define FILENAME ".test.extract"

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

int main(int argc, char *argv[]) {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab, *c, *c2;
  struct mscabd_file *file;
  FILE *fh;
  int err;

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  /* if self-test reveals an error */
  MSPACK_SYS_SELFTEST(err);
  if (err) return 1;

  if ((cabd = mspack_create_cab_decompressor(NULL))) {
    for (argv++; *argv; argv++) {
      printf("*** %s\n", *argv);
      if ((cab = cabd->open(cabd, *argv))) {

	for (c = cab; c && (c->flags & MSCAB_HDR_PREVCAB); c = c->prevcab) {
	  if (!(c2 = cabd->open(cabd, c->prevname))) {
	    printf("prev cab open error %d\n", cabd->last_error(cabd));
	  }
	  if (cabd->prepend(cabd, c, c2)) {
	    printf("cab prepend error %d\n", cabd->last_error(cabd));
	  }
	}

	for (c = cab; c && (c->flags & MSCAB_HDR_NEXTCAB); c = c->nextcab) {
	  if (!(c2 = cabd->open(cabd, c->nextname))) {
	    printf("next cab open error %d\n", cabd->last_error(cabd));
	  }
	  if (cabd->append(cabd, c, c2)) {
	    printf("cab append error %d\n", cabd->last_error(cabd));
	  }
	}

	for (file = cab->files; file; file = file->next ) {
	  if ((cabd->extract(cabd, file, FILENAME))) {
	    printf("%s: %s extract error %d\n", *argv,
		   file->filename, cabd->last_error(cabd));
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
	cabd->close(cabd, cab);
      }
      else {
	fprintf(stderr, "cab open error %d\n", cabd->last_error(cabd));
      }
    }
    mspack_destroy_cab_decompressor(cabd);
  }
  else {
    fprintf(stderr, "can't make decompressor\n");
  }
  return 0;
}
