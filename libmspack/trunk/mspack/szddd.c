/* This file is part of libmspack.
 * (C) 2003-2004 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

/* SZDD decompression implementation */

#include <system.h>
#include <szdd.h>

/* prototypes */
static struct msszddd_header *szddd_open(
  struct msszdd_decompressor *self, char *filename);
static void szddd_close(
  struct msszdd_decompressor *self, struct msszddd_header *szdd);
static int szddd_read_headers(
  struct mspack_system *sys, struct mspack_file *fh,
  struct msszddd_header *szdd);
static int szddd_extract(
  struct msszdd_decompressor *self, struct msszddd_header *szdd,
  char *filename);
static int szddd_decompress(
  struct msszdd_decompressor *self, char *input, char *output);
static int szddd_error(
  struct msszdd_decompressor *self);

/***************************************
 * MSPACK_CREATE_SZDD_DECOMPRESSOR
 ***************************************
 * constructor
 */
struct msszdd_decompressor *
  mspack_create_szdd_decompressor(struct mspack_system *sys)
{
  struct msszdd_decompressor_p *this = NULL;

  if (!sys) sys = mspack_default_system;
  if (!mspack_valid_system(sys)) return NULL;

  if ((this = sys->alloc(sys, sizeof(struct msszdd_decompressor_p)))) {
    this->base.open       = &szddd_open;
    this->base.close      = &szddd_close;
    this->base.extract    = &szddd_extract;
    this->base.decompress = &szddd_decompress;
    this->base.last_error = &szddd_error;
    this->system          = sys;
    this->error           = MSPACK_ERR_OK;
  }
  return (struct msszdd_decompressor *) this;
}

/***************************************
 * MSPACK_DESTROY_SZDD_DECOMPRESSOR
 ***************************************
 * destructor
 */
void mspack_destroy_szdd_decompressor(struct msszdd_decompressor *self)
{
  struct msszdd_decompressor_p *this = (struct msszdd_decompressor_p *) self;
  if (this) {
    struct mspack_system *sys = this->system;
    sys->free(this);
  }
}

/***************************************
 * SZDDD_OPEN
 ***************************************
 * opens an SZDD file without decompressing, reads header
 */
static struct msszddd_header *szddd_open(struct msszdd_decompressor *self,
					 char *filename)
{
  struct msszdd_decompressor_p *this = (struct msszdd_decompressor_p *) self;
  struct msszddd_header_p *szdd = NULL;
  struct mspack_system *sys;
  struct mspack_file *fh;
  int error;

  if (!this) return NULL;
  sys = this->system;

  if ((fh = sys->open(sys, filename, MSPACK_SYS_OPEN_READ))) {
    if ((szdd = sys->alloc(sys, sizeof(struct msszddd_header_p)))) {
      szdd->fh = fh;
      error = szddd_read_headers(sys, fh, (struct msszddd_header *) szdd);
      if (error) {
	szddd_close(self, (struct msszddd_header *) szdd);
	szdd = NULL;
      }
      this->error = error;
    }
    else {
      this->error = MSPACK_ERR_NOMEMORY;
    }
    sys->close(fh);
  }
  else {
    this->error = MSPACK_ERR_OPEN;
  }
  return (struct msszddd_header *) szdd;
}

/***************************************
 * SZDDD_CLOSE
 ***************************************
 * closes an SZDD file
 */
static void szddd_close(struct msszdd_decompressor *self,
			struct msszddd_header *szdd)
{
  struct msszdd_decompressor_p *this = (struct msszdd_decompressor_p *) self;
  struct msszddd_header_p *szdd_p = (struct msszddd_header_p *) szdd;

  if (!this) return;
  if (!this->system) {
    this->error = MSPACK_ERR_ARGS;
    return;
  }

  /* close the file handle associated */
  if (szdd_p->fh) {
    this->system->close(szdd_p->fh);
  }

  /* free the memory associated */
  this->system->free(szdd);

  this->error = MSPACK_ERR_OK;
}

/***************************************
 * SZDDD_READ_HEADERS
 ***************************************
 * reads the headers of an SZDD format file
 */
static unsigned char szdd_signature[8] = {
  0x53, 0x5A, 0x44, 0x44, 0x88, 0xF0, 0x27, 0x33
};

static int szddd_read_headers(struct mspack_system *sys,
			      struct mspack_file *fh,
			      struct msszddd_header *szdd)
{
  unsigned char buf[szddhead_SIZEOF];

  szdd->length = 0;
  szdd->missing_char = '\0';

  /* read header */
  if (sys->read(fh, &buf[0], szddhead_SIZEOF) != szddhead_SIZEOF) {
    return MSPACK_ERR_READ;
  }

  /* check signature */
  if ((mspack_memcmp(&buf[szddhead_Signature], &szdd_signature[0], 8) != 0)) {
    return MSPACK_ERR_SIGNATURE;
  }

  /* check compression method */
  if (buf[szddhead_CompType] != SZDD_COMPTYPE_A) {
    return MSPACK_ERR_DATAFORMAT;
  }

  /* read missing character from filename */
  szdd->missing_char = buf[szddhead_FileChar];

  /* read decompressed length of file */
  szdd->length = EndGetI32(&buf[szddhead_FileLength]);
  return MSPACK_ERR_OK;
}

/***************************************
 * SZDDD_EXTRACT
 ***************************************
 * decompresses an SZDD file
 */
static int szddd_extract(struct msszdd_decompressor *self,
			 struct msszddd_header *szdd, char *filename)
{
  struct msszdd_decompressor_p *this = (struct msszdd_decompressor_p *) self;
  struct mspack_system *sys;
  struct mspack_file *outfh;

  if (!this) return MSPACK_ERR_ARGS;
  sys = this->system;
}

/***************************************
 * SZDDD_DECOMPRESS
 ***************************************
 * unpacks directly from input to output
 */
static int szddd_decompress(struct msszdd_decompressor *self,
			    char *input, char *output)
{
  struct msszdd_decompressor_p *this = (struct msszdd_decompressor_p *) self;
  struct msszddd_header_p szdd;
  struct mspack_system *sys;
  struct mspack_file *fh;
  int error = MSPACK_ERR_OK;

  if (!this) return MSPACK_ERR_ARGS;
  sys = this->system;

  if ((fh = sys->open(sys, input, MSPACK_SYS_OPEN_READ))) {
    szdd.fh = fh;
    error = szddd_read_headers(sys, fh, (struct msszddd_header *) &szdd);
    if (error == MSPACK_ERR_OK) {
      error = szddd_extract(self, (struct msszddd_header *) &szdd, output);
    }
    sys->close(fh);
  }
  else {
    error = MSPACK_ERR_OPEN;
  }
  return this->error = error;
}

/***************************************
 * SZDDD_ERROR
 ***************************************
 * returns the last error that occurred
 */
static int szddd_error(struct msszdd_decompressor *self)
{
  struct msszdd_decompressor_p *this = (struct msszdd_decompressor_p *) self;
  return (this) ? this->error : MSPACK_ERR_ARGS;
}
