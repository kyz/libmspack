/* This file is part of libmspack.
 * (C) 2003-2004 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

/* CHM decompression implementation */

#include <mspack.h>
#include <system.h>
#include <chm.h>

/* prototypes */
static struct mschmd_header * chmd_open(
  struct mschm_decompressor *base, char *filename);
static struct mschmd_header * chmd_fast_open(
  struct mschm_decompressor *base, char *filename);
static struct mschmd_header *chmd_real_open(
  struct mschm_decompressor *base, char *filename, int entire);
static void chmd_close(
  struct mschm_decompressor *base, struct mschmd_header *chm);
static int chmd_read_headers(
  struct mspack_system *sys, struct mspack_file *fh,
  struct mschmd_header *chm, int entire);
static int chmd_fast_find(
  struct mschm_decompressor *base, struct mschmd_header *chm, char *filename,
  struct mschmd_file *f_ptr, int f_size);
static int chmd_extract(
  struct mschm_decompressor *base, struct mschmd_file *file, char *filename);
static int chmd_sys_write(
  struct mspack_file *file, void *buffer, int bytes);
static int chmd_init_decomp(
  struct mschm_decompressor_p *this, struct mschmd_file *file);
static unsigned char *read_sys_file(
  struct mschm_decompressor_p *this, struct mschmd_file *file);
static int chmd_error(
  struct mschm_decompressor *base);

/* CHM uses 64-bit offsets in its files. These can only be completely
 * supported if the library is compiled to support large files (off_t >=
 * 64 bits). If not, the library will work as normal using only 32-bit
 * arithmetic, but if an offset greater than 2Gb is detected, an error
 * message indicating the library can't support the file will be returned.
 */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#endif
#if (_FILE_OFFSET_BITS < 64)
static char *largefile_msg =
  "library not compiled to support large files.";
#define LU "%lu"
#else
#define LU "%llu"
#endif

/* filenames of the three essential system files needed for decompression */
static char *content_name = "::DataSpace/Storage/MSCompressed/Content";
static char *control_name = "::DataSpace/Storage/MSCompressed/ControlData";
static char *rtable_name  = "::DataSpace/Storage/MSCompressed/Transform/"
  "{7FC28940-9D31-11D0-9B27-00A0C91E9C7C}/InstanceData/ResetTable";


/***************************************
 * MSPACK_CREATE_CHM_DECOMPRESSOR
 ***************************************
 * constructor
 */
struct mschm_decompressor *
  mspack_create_chm_decompressor(struct mspack_system *sys)
{
  struct mschm_decompressor_p *this = NULL;

  if (!sys) sys = mspack_default_system;
  if (!mspack_valid_system(sys)) return NULL;

  if ((this = sys->alloc(sys, sizeof(struct mschm_decompressor_p)))) {
    this->base.open       = &chmd_open;
    this->base.close      = &chmd_close;
    this->base.extract    = &chmd_extract;
    this->base.last_error = &chmd_error;
    this->base.fast_open  = &chmd_fast_open;
    this->base.fast_find  = &chmd_fast_find;
    this->system          = sys;
    this->error           = MSPACK_ERR_OK;
    this->d               = NULL;
  }
  return (struct mschm_decompressor *) this;
}

/***************************************
 * MSPACK_DESTROY_CAB_DECOMPRESSOR
 ***************************************
 * destructor
 */
void mspack_destroy_chm_decompressor(struct mschm_decompressor *base) {
  struct mschm_decompressor_p *this = (struct mschm_decompressor_p *) base;
  if (this) {
    struct mspack_system *sys = this->system;
    if (this->d) {
      if (this->d->infh)  sys->close(this->d->infh);
      if (this->d->state) lzxd_free(this->d->state);
      sys->free(this->d);
    }
    sys->free(this);
  }
}

/***************************************
 * CHMD_OPEN
 ***************************************
 * opens a file and tries to read it as a CHM file.
 * Calls chmd_real_open() with entire=1.
 */
static struct mschmd_header *chmd_open(struct mschm_decompressor *base,
				       char *filename)
{
  return chmd_real_open(base, filename, 1);
}

/***************************************
 * CHMD_FAST_OPEN
 ***************************************
 * opens a file and tries to read it as a CHM file, but does not read
 * the file headers. Calls chmd_real_open() with entire=0
 */
static struct mschmd_header *chmd_fast_open(struct mschm_decompressor *base,
					    char *filename)
{
  return chmd_real_open(base, filename, 0);
}

/***************************************
 * CHMD_REAL_OPEN
 ***************************************
 * the real implementation of chmd_open() and chmd_fast_open(). It simply
 * passes the "entire" parameter to chmd_read_headers(), which will then
 * either read all headers, or a bare mininum.
 */
static struct mschmd_header *chmd_real_open(struct mschm_decompressor *base,
					    char *filename, int entire)
{
  struct mschm_decompressor_p *this = (struct mschm_decompressor_p *) base;
  struct mschmd_header *chm = NULL;
  struct mspack_system *sys;
  struct mspack_file *fh;
  int error;

  if (!base) return NULL;
  sys = this->system;

  if ((fh = sys->open(sys, filename, MSPACK_SYS_OPEN_READ))) {
    if ((chm = sys->alloc(sys, sizeof(struct mschmd_header)))) {
      chm->filename = filename;
      error = chmd_read_headers(sys, fh, chm, entire);
      if (error) {
	chmd_close(base, chm);
	chm = NULL;
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
  return chm;
}

/***************************************
 * CHMD_CLOSE
 ***************************************
 * frees all memory associated with a given mschmd_header
 */
static void chmd_close(struct mschm_decompressor *base,
		       struct mschmd_header *chm)
{
  struct mschm_decompressor_p *this = (struct mschm_decompressor_p *) base;
  struct mschmd_file *fi, *nfi;
  struct mspack_system *sys;

  if (!base) return;
  sys = this->system;

  this->error = MSPACK_ERR_OK;

  /* free files */
  for (fi = chm->files; fi; fi = nfi) {
    nfi = fi->next;
    sys->free(fi);
  }
  for (fi = chm->sysfiles; fi; fi = nfi) {
    nfi = fi->next;
    sys->free(fi);
  }

  /* if this CHM was being decompressed, free decompression state */
  if (this->d && (this->d->chm == chm)) {
    if (this->d->infh) sys->close(this->d->infh);
    if (this->d->state) lzxd_free(this->d->state);
    sys->free(this->d);
    this->d = NULL;
  }

  sys->free(chm);
}

/***************************************
 * CHMD_READ_HEADERS
 ***************************************
 * reads the basic CHM file headers. If the "entire" parameter is
 * non-zero, all file entries will also be read. fills out a pre-existing
 * mschmd_header structure, allocates memory for files as necessary
 */

/* The GUIDs found in CHM headers */
static unsigned char guids[32] = {
  /* {7C01FD10-7BAA-11D0-9E0C-00A0-C922-E6EC} */
  0x10, 0xFD, 0x01, 0x7C, 0xAA, 0x7B, 0xD0, 0x11,
  0x9E, 0x0C, 0x00, 0xA0, 0xC9, 0x22, 0xE6, 0xEC,
  /* {7C01FD11-7BAA-11D0-9E0C-00A0-C922-E6EC} */
  0x11, 0xFD, 0x01, 0x7C, 0xAA, 0x7B, 0xD0, 0x11,
  0x9E, 0x0C, 0x00, 0xA0, 0xC9, 0x22, 0xE6, 0xEC
};

static int chmd_read_headers(struct mspack_system *sys, struct mspack_file *fh,
			     struct mschmd_header *chm, int entire)
{
  unsigned int num_chunks, num_entries, section, name_len, x;
  unsigned char buf[0x54], *chunk = NULL, *name, *p;
  struct mschmd_file *fi, *link = NULL;
  off_t offset, length;

  /* initialise pointers */
  chm->files         = NULL;
  chm->sysfiles      = NULL;
  chm->sec0.base.chm = chm;
  chm->sec0.base.id  = 0;
  chm->sec1.base.chm = chm;
  chm->sec1.base.id  = 1;
  chm->sec1.content  = NULL;
  chm->sec1.control  = NULL;
  chm->sec1.rtable   = NULL;


  /* read the first header */
  if (sys->read(fh, &buf[0], chmhead_SIZEOF) != chmhead_SIZEOF) {
    return MSPACK_ERR_READ;
  }

  /* check ITSF signature */
  if (EndGetI32(&buf[chmhead_Signature]) != 0x46535449) {
    return MSPACK_ERR_SIGNATURE;
  }

  /* check both header GUIDs */
  if (mspack_memcmp(&buf[chmhead_GUID1], &guids[0], 32) != 0) {
    D(("incorrect GUIDs"))
    return MSPACK_ERR_SIGNATURE;
  }

  chm->version   = EndGetI32(&buf[chmhead_Version]);
  chm->timestamp = EndGetM32(&buf[chmhead_Timestamp]);
  chm->language  = EndGetI32(&buf[chmhead_LanguageID]);
  if (chm->version > 3) {
    sys->message(fh, "WARNING; CHM version > 3");
  }

  /* read the header section table */
  if (sys->read(fh, &buf[0], chmhst3_SIZEOF) != chmhst3_SIZEOF) {
    return MSPACK_ERR_READ;
  }

  /* chmhst3_OffsetCS0 does not exist in version 1 or 2 CHM files.
   * The offset will be corrected later, once HS1 is read.
   */
#if (_FILE_OFFSET_BITS >= 64)
  offset           = EndGetI64(&buf[chmhst_OffsetHS0]);
  chm->dir_offset  = EndGetI64(&buf[chmhst_OffsetHS1]);
  chm->sec0.offset = EndGetI64(&buf[chmhst3_OffsetCS0]);
#else
  offset           = EndGetI32(&buf[chmhst_OffsetHS0]);
  chm->dir_offset  = EndGetI32(&buf[chmhst_OffsetHS1]);
  chm->sec0.offset = EndGetI32(&buf[chmhst3_OffsetCS0]);

  if (EndGetI32(&buf[chmhst_OffsetHS0  + 4]) ||
      EndGetI32(&buf[chmhst_OffsetHS1  + 4]) ||
      EndGetI32(&buf[chmhst3_OffsetCS0 + 4]) ||
      (offset           & 0x80000000) ||
      (chm->dir_offset  & 0x80000000) ||
      (chm->sec0.offset & 0x80000000) )
  {
    sys->message(fh, largefile_msg);
    return MSPACK_ERR_DATAFORMAT;
  }
#endif

  /* seek to header section 0 */
  if (sys->seek(fh, offset, MSPACK_SYS_SEEK_START)) {
    return MSPACK_ERR_SEEK;
  }

  /* read header section 0 */
  if (sys->read(fh, &buf[0], chmhs0_SIZEOF) != chmhs0_SIZEOF) {
    return MSPACK_ERR_READ;
  }

#if (_FILE_OFFSET_BITS >= 64)
  chm->length = EndGetI64(&buf[chmhs0_FileLen]);
#else
  chm->length = EndGetI32(&buf[chmhs0_FileLen]);
  if (EndGetI32(&buf[chmhs0_FileLen+4]) || (chm->length & 0x80000000)) {
    sys->message(fh, largefile_msg);
    return MSPACK_ERR_DATAFORMAT;
  }
#endif

  /* seek to header section 1 */
  if (sys->seek(fh, chm->dir_offset, MSPACK_SYS_SEEK_START)) {
    return MSPACK_ERR_SEEK;
  }

  /* read header section 1 */
  if (sys->read(fh, &buf[0], chmhs1_SIZEOF) != chmhs1_SIZEOF) {
    return MSPACK_ERR_READ;
  }

  chm->dir_offset = sys->tell(fh);
  chm->chunk_size = EndGetI32(&buf[chmhs1_ChunkSize]);
  chm->density    = EndGetI32(&buf[chmhs1_Density]);
  chm->depth      = EndGetI32(&buf[chmhs1_Depth]);
  chm->index_root = EndGetI32(&buf[chmhs1_IndexRoot]);
  chm->num_chunks = EndGetI32(&buf[chmhs1_NumChunks]);

  if (chm->version < 3) {
    /* versions before 3 don't have chmhst3_OffsetCS0 */
    chm->sec0.offset = chm->dir_offset + (chm->chunk_size * chm->num_chunks);
  }

  /* if we are doing a quick read, stop here! */
  if (!entire) {
    return MSPACK_ERR_OK;
  }

  /* seek to the first PMGL chunk, and reduce the number of chunks to read */
  if ((x = EndGetI32(&buf[chmhs1_FirstPMGL]))) {
    if (sys->seek(fh,(off_t) (x * chm->chunk_size), MSPACK_SYS_SEEK_CUR)) {
      return MSPACK_ERR_SEEK;
    }
  }
  num_chunks = EndGetI32(&buf[chmhs1_LastPMGL]) - x + 1;

  if (!(chunk = sys->alloc(sys, chm->chunk_size))) {
    return MSPACK_ERR_NOMEMORY;
  }

  /* read and process all chunks from FirstPMGL to LastPMGL */
  while (num_chunks--) {
    if (sys->read(fh, chunk, (int)chm->chunk_size) != (int)chm->chunk_size) {
      sys->free(chunk);
      return MSPACK_ERR_READ;
    }

    /* process only directory (PMGL) chunks */
    if (EndGetI32(&chunk[pmgl_Signature]) != 0x4C474D50) continue;

    num_entries = EndGetI16(&chunk[chm->chunk_size-2]);
    p = &chunk[pmgl_Entries];
    while (num_entries--) {
      if (p > &chunk[chm->chunk_size]) {
	D(("read beyond end of chunk"))
	sys->free(chunk);
	return MSPACK_ERR_DATAFORMAT;
      }

      name_len = 0; section  = 0;
      offset   = 0; length   = 0;
      do { name_len = (name_len << 7) | (*p & 0x7F); } while (*p++ & 0x80);
      name = p; p += name_len;
      do { section  = (section  << 7) | (*p & 0x7F); } while (*p++ & 0x80);
      do { offset   = (offset   << 7) | (*p & 0x7F); } while (*p++ & 0x80);
      do { length   = (length   << 7) | (*p & 0x7F); } while (*p++ & 0x80);

      if ((offset == 0) && (length == 0)) {
	/* null file -- used to store directory names. Why? */
	continue;
      }

      if (section > 1) {
	sys->message(fh, "invalid section number '%u'.", section);
	continue;
      }

      if (!(fi = sys->alloc(sys, sizeof(struct mschmd_file) + name_len + 1))) {
	sys->free(chunk);
	return MSPACK_ERR_NOMEMORY;
      }

      fi->next     = NULL;
      fi->filename = (char *) &fi[1];
      fi->section  = ((section == 0) ? (struct mschmd_section *) (&chm->sec0)
		                     : (struct mschmd_section *) (&chm->sec1));
      fi->offset   = offset;
      fi->length   = length;
      sys->copy(name, fi->filename, (size_t) name_len);
      fi->filename[name_len] = '\0';

      if (name[0] == ':' && name[1] == ':') {
	/* system file */
	if (mspack_memcmp(&name[2], &content_name[2], 31) == 0) {
	  if (mspack_memcmp(&name[33], &content_name[33], 8) == 0) {
	    chm->sec1.content = fi;
	  }
	  else if (mspack_memcmp(&name[33], &control_name[33], 11) == 0) {
	    chm->sec1.control = fi;
	  }
	  else if (mspack_memcmp(&name[33], &rtable_name[33], 72) == 0) {
	    chm->sec1.rtable = fi;
	  }
	}
	fi->next = chm->sysfiles;
	chm->sysfiles = fi;
      }
      else {
	/* normal file */
	if (link) link->next = fi; else chm->files = fi;
	link = fi;
      }
    }
  }
  sys->free(chunk);
  return MSPACK_ERR_OK;
}

/***************************************
 * CABD_FAST_FIND
 ***************************************
 * uses PMGI index chunks and quickref data to quickly locate a file
 * directly from the on-disk index.
 */
static int chmd_fast_find(struct mschm_decompressor *base,
			  struct mschmd_header *chm, char *filename,
			  struct mschmd_file *f_ptr, int f_size)
{
  struct mschm_decompressor_p *this = (struct mschm_decompressor_p *) base;
  struct mspack_system *sys;
  struct mspack_file *fh;
  unsigned int block;
  unsigned char *chunk;

  if (!this || !chm || !f_ptr || (f_size != sizeof(struct mschmd_file))) {
    return MSPACK_ERR_ARGS;
  }
  sys = this->system;

  if (!(chunk = sys->alloc(sys, chm->chunk_size))) {
    return MSPACK_ERR_NOMEMORY;
  }

  if (!(fh = sys->open(sys, chm->filename, MSPACK_SYS_OPEN_READ))) {
    sys->free(chunk);
    return MSPACK_ERR_OPEN;
  }

  /* go through all PMGI blocks (if there are any present) */
  block = (chm->index_root >= 0) ? chm->index_root : 0;
  do {
    /* seek to block and read it */
    if (sys->seek(fh, (off_t) (chm->dir_offset + (block * chm->chunk_size)),
		  MSPACK_SYS_SEEK_CUR))
    {
      sys->free(chunk);
      sys->close(fh);
      return MSPACK_ERR_SEEK;
    }
    if (sys->read(fh, chunk, (int)chm->chunk_size) != (int)chm->chunk_size) {
      sys->free(chunk);
      sys->close(fh);
      return MSPACK_ERR_READ;
    }

    /* check the signature. Is is PGML or PGMI? */
    if (!((chunk[0] == 'P') && (chunk[1] == 'G') && (chunk[2] == 'M') &&
	  ((chunk[3] == 'L') || (chunk[3] == 'I'))))
    {
      sys->free(chunk);
      sys->close(fh);
      return MSPACK_ERR_DATAFORMAT;
    }
    /* if PGML, we have found the listing page! */
    if (chunk[3] == 'L') {
      /* LOOP EXIT POINT */
      break;
    }

    /* perform binary search on quickrefs */
    /* perform linear search on quickref segment */

  } while (1); /* see LOOP EXIT POINT above */

  /* a loop through all blocks, if chm->index_root < 0 */
  /* otherwise just this block */

  /* perform binary search on quickrefs */
  /* perform linear search on quickref segment */
  sys->close(fh);
  sys->free(chunk);
}


/***************************************
 * CABD_EXTRACT
 ***************************************
 * extracts a file from a CHM helpfile
 */
static int chmd_extract(struct mschm_decompressor *base,
			struct mschmd_file *file, char *filename)
{
  struct mschm_decompressor_p *this = (struct mschm_decompressor_p *) base;
  struct mspack_system *sys;
  struct mschmd_header *chm;
  struct mspack_file *fh;
  off_t bytes;

  if (!this) return MSPACK_ERR_ARGS;
  if (!file || !file->section) return this->error = MSPACK_ERR_ARGS;
  sys = this->system;
  chm = file->section->chm;

  /* create decompression state if it doesn't exist */
  if (!this->d) {
    this->d = sys->alloc(sys, sizeof(struct mschmd_decompress_state));
    if (!this->d) return this->error = MSPACK_ERR_NOMEMORY;
    this->d->chm       = chm;
    this->d->offset    = 0;
    this->d->state     = NULL;
    this->d->sys       = *sys;
    this->d->sys.write = &chmd_sys_write;
    this->d->infh      = NULL;
    this->d->outfh     = NULL;
  }

  /* open input chm file if not open, or the open one is a different chm */
  if (!this->d->infh || (this->d->chm != chm)) {
    if (this->d->infh)  sys->close(this->d->infh);
    if (this->d->state) lzxd_free(this->d->state);
    this->d->chm    = chm;
    this->d->offset = 0;
    this->d->state  = NULL;
    this->d->infh   = sys->open(sys, chm->filename, MSPACK_SYS_OPEN_READ);
    if (!this->d->infh) return this->error = MSPACK_ERR_OPEN;
  }

  /* open file for output */
  if (!(fh = sys->open(sys, filename, MSPACK_SYS_OPEN_WRITE))) {
    return this->error = MSPACK_ERR_OPEN;
  }

  /* if file is empty, simply creating it is enough */
  if (!file->length) {
    sys->close(fh);
    return this->error = MSPACK_ERR_OK;
  }

  this->error = MSPACK_ERR_OK;

  switch (file->section->id) {
  case 0: /* Uncompressed section file */
    /* simple seek + copy */
    if (sys->seek(this->d->infh, file->section->chm->sec0.offset
		  + file->offset, MSPACK_SYS_SEEK_START))
    {
      this->error = MSPACK_ERR_SEEK;
    }
    else {
      unsigned char buf[512];
      off_t length = file->length;
      while (length > 0) {
	int run = sizeof(buf);
	if ((off_t)run > length) run = (int)length;
	if (sys->read(this->d->infh, &buf[0], run) != run) {
	  this->error = MSPACK_ERR_READ;
	  break;
	}
	if (sys->write(fh, &buf[0], run) != run) {
	  this->error = MSPACK_ERR_WRITE;
	  break;
	}
	length -= run;
      }
    }
    break;

  case 1: /* MSCompressed section file */
    /* (re)initialise compression state if we it is not yet initialised,
     * or we have advanced too far and have to backtrack
     */
    if (!this->d->state || (file->offset < this->d->offset)) {
      if (this->d->state) {
	lzxd_free(this->d->state);
	this->d->state = NULL;
      }
      if (chmd_init_decomp(this, file)) break;
    }

    /* seek to input data */
    if (sys->seek(this->d->infh, this->d->inoffset, MSPACK_SYS_SEEK_START)) {
      this->error = MSPACK_ERR_SEEK;
      break;
    }

    /* get to correct offset. */
    this->d->outfh = NULL;
    if ((bytes = file->offset - this->d->offset)) {
      D(("skip to offset " LU " by decoding " LU " bytes", file->offset, bytes))
      this->error = lzxd_decompress(this->d->state, bytes);
    }

    /* if getting to the correct offset was error free, unpack file */
    if (!this->error) {
      D(("decode " LU " bytes from offset " LU, file->length, file->offset))
      this->d->outfh = fh;
      this->error = lzxd_decompress(this->d->state, file->length);
    }

    /* save offset in input source stream, in case there is a section 0
     * file between now and the next section 1 file extracted */
    this->d->inoffset = sys->tell(this->d->infh);

    /* if an LZX error occured, the LZX decompressor is now useless */
    if (this->error) {
      if (this->d->state) lzxd_free(this->d->state);
      this->d->state = NULL;
    }
    break;
  }

  sys->close(fh);
  return this->error;
}

/***************************************
 * CHMD_SYS_WRITE
 ***************************************
 * chmd_sys_write is the internal writer function which the decompressor
 * uses. If either writes data to disk (this->d->outfh) with the real
 * sys->write() function, or does nothing with the data when
 * this->d->outfh == NULL. advances this->d->offset.
 */
static int chmd_sys_write(struct mspack_file *file, void *buffer, int bytes) {
  struct mschm_decompressor_p *this = (struct mschm_decompressor_p *) file;
  this->d->offset += bytes;
  if (this->d->outfh) {
    return this->system->write(this->d->outfh, buffer, bytes);
  }
  return bytes;
}

/***************************************
 * CHMD_INIT_DECOMP
 ***************************************
 * Initialises the LZX decompressor to decompress the compressed stream,
 * from the nearest reset offset and length that is needed for the given
 * file.
 */
static int chmd_init_decomp(struct mschm_decompressor_p *this,
			    struct mschmd_file *file)
{
  int window_size, window_bits, reset_interval, entry, tablepos, entrysize;
  struct mspack_system *sys = this->system;
  struct mschmd_sec_mscompressed *sec;
  struct mschmd_file sysfile;
  unsigned char *data;
  off_t length;

  sec = (struct mschmd_sec_mscompressed *) file->section;

  /* ensure we have a mscompressed content section */
  if (!sec->content) {
    if (chmd_fast_find((struct mschm_decompressor *) this, sec->base.chm,
			content_name, &sysfile, sizeof(sysfile)))
      return this->error = MSPACK_ERR_DATAFORMAT;

    if (!(sec->content = sys->alloc(sys, sizeof(sysfile))))
      return this->error = MSPACK_ERR_NOMEMORY;

    *sec->content = sysfile;
    sec->content->filename = content_name;
    sec->content->next = sec->base.chm->sysfiles;
    sec->base.chm->sysfiles = sec->content;
  }

  /* ensure we have a ControlData file */
  if (!sec->control) {
    if (chmd_fast_find((struct mschm_decompressor *) this, sec->base.chm,
			control_name, &sysfile, sizeof(sysfile)))
      return this->error = MSPACK_ERR_DATAFORMAT;

    if (!(sec->control = sys->alloc(sys, sizeof(sysfile))))
      return this->error = MSPACK_ERR_NOMEMORY;

    *sec->control = sysfile;
    sec->control->filename = content_name;
    sec->control->next = sec->base.chm->sysfiles;
    sec->base.chm->sysfiles = sec->control;
  }

  /* ensure we have a reset table */
  if (!sec->rtable) {
    if (chmd_fast_find((struct mschm_decompressor *) this, sec->base.chm,
			rtable_name, &sysfile, sizeof(sysfile)))
      return this->error = MSPACK_ERR_DATAFORMAT;

    if (!(sec->rtable = sys->alloc(sys, sizeof(sysfile))))
      return this->error = MSPACK_ERR_NOMEMORY;

    *sec->rtable = sysfile;
    sec->rtable->filename = content_name;
    sec->rtable->next = sec->base.chm->sysfiles;
    sec->base.chm->sysfiles = sec->rtable;
  }

  /* read ControlData */
  if (!(data = read_sys_file(this, sec->control))) {
    D(("can't read mscompressed control data file"))
    return this->error;
  }

  /* check LZXC signature */
  if (EndGetI32(&data[lzxcd_Signature]) != 0x43585A4C) {
    sys->free(data);
    return this->error = MSPACK_ERR_SIGNATURE;
  }

  /* read reset_interval and window_size and validate version number */
  switch (EndGetI32(&data[lzxcd_Version])) {
  case 1:
    reset_interval = EndGetI32(&data[lzxcd_ResetInterval]);
    window_size    = EndGetI32(&data[lzxcd_WindowSize]);
    break;
  case 2:
    reset_interval = EndGetI32(&data[lzxcd_ResetInterval]) * LZX_FRAME_SIZE;
    window_size    = EndGetI32(&data[lzxcd_WindowSize])    * LZX_FRAME_SIZE;
    break;
  default:
    D(("bad controldata version"))
    sys->free(data);
    return this->error = MSPACK_ERR_DATAFORMAT;
  }

  /* free ControlData */
  sys->free(data);

  /* find window_bits from window_size */
  switch (window_size) {
  case 0x008000: window_bits = 15; break;
  case 0x010000: window_bits = 16; break;
  case 0x020000: window_bits = 17; break;
  case 0x040000: window_bits = 18; break;
  case 0x080000: window_bits = 19; break;
  case 0x100000: window_bits = 20; break;
  case 0x200000: window_bits = 21; break;
  default:
    D(("bad controldata window size"))
    return this->error = MSPACK_ERR_DATAFORMAT;
  }

  /* validate reset_interval */
  if (reset_interval % LZX_FRAME_SIZE) {
    D(("bad controldata reset interval"))
    return this->error = MSPACK_ERR_DATAFORMAT;
  }

  /* read ResetTable file */
  if (!(data = read_sys_file(this, sec->rtable))) {
    D(("can't read reset table"))
    return this->error;
  }

  /* check sanity of reset table */
  if (EndGetI32(&data[lzxrt_FrameLen]) != LZX_FRAME_SIZE) {
    D(("bad resettable frame length"))
    sys->free(data);
    return this->error = MSPACK_ERR_DATAFORMAT;
  }

  /* get the uncompressed length of the LZX stream */
#if (_FILE_OFFSET_BITS >= 64)
  length = EndGetI64(&data[lzxrt_UncompLen]);
#else
  length = EndGetI32(&data[lzxrt_UncompLen]);
  if (EndGetI32(&data[lzxrt_UncompLen+4]) || (length < 0)) {
    sys->message(this->d->infh, largefile_msg);
    sys->free(data);
    return this->error = MSPACK_ERR_DATAFORMAT;
  }
#endif

  /* FIXME: urgh, urgh, urgh:
   * the uncompressed length given in the reset table is not actually honest.
   * the compressed stream is padded out from the uncompressed length up to
   * the next reset interval
   */
  length += reset_interval - 1;
  length &= -reset_interval;

  /* pick nearest reset interval below the offset we seek to start from */
  entry = file->offset / reset_interval;
  /* convert from reset interval multiple (usually 64k) to 32k frames */
  entry *= reset_interval / LZX_FRAME_SIZE;

  /* ensure reset table entry for this offset exists */
  entrysize = EndGetI32(&data[lzxrt_EntrySize]);
  tablepos = EndGetI32(&data[lzxrt_TableOffset]) + (entry * entrysize);
  if (entry >= EndGetI32(&data[lzxrt_NumEntries]) ||
      ((entrysize != 4) && (entrysize != 8)) ||
      ((tablepos + entrysize) > sec->rtable->length))
  {
    D(("bad resettable reset interval choice"))
    sys->free(data);
    return this->error = MSPACK_ERR_DATAFORMAT;
  }

  /* get offset of compressed data stream:
   * = offset of uncompressed section from start of file
   * + offset of compressed stream from start of uncompressed section
   * + offset of chosen reset interval from start of compressed stream */
  this->d->inoffset = file->section->chm->sec0.offset + sec->content->offset;
  switch (entrysize) {
  case 4:
    this->d->inoffset += EndGetI32(&data[tablepos]);
    break;
  case 8:
#if (_FILE_OFFSET_BITS >= 64)
    this->d->inoffset += EndGetI64(&data[tablepos]);
#else
    this->d->inoffset += EndGetI32(&data[tablepos]);
    if (EndGetI32(&data[tablepos+4])) {
      sys->message(this->d->infh, largefile_msg);
      sys->free(data);
      return this->error = MSPACK_ERR_DATAFORMAT;
    }
#endif
    break;
  }

  /* free the reset table */
  sys->free(data);

  /* set start offset and overall remaining stream length */
  this->d->offset = entry * LZX_FRAME_SIZE;
  length -= this->d->offset;

  /* initialise LZX stream */
  D(("starting decode from " LU, this->d->offset))
  this->d->state = lzxd_init(&this->d->sys, this->d->infh,
			     (struct mspack_file *) this, window_bits,
			     reset_interval / LZX_FRAME_SIZE,
			     4096, length);
  if (!this->d->state) this->error = MSPACK_ERR_NOMEMORY;
  return this->error;
}

/***************************************
 * READ_SYS_FILE
 ***************************************
 * Allocates memory for a section 0 (uncompressed) file and reads it into
 * memory.
 */
static unsigned char *read_sys_file(struct mschm_decompressor_p *this,
				    struct mschmd_file *file)
{
  struct mspack_system *sys = this->system;
  unsigned char *data = NULL;
  int len;

  if (!file || !file->section || (file->section->id != 0)) {
    this->error = MSPACK_ERR_DATAFORMAT;
    return NULL;
  }

  len = (int) file->length;

  if (!(data = sys->alloc(sys, (size_t) len))) {
    this->error = MSPACK_ERR_NOMEMORY;
    return NULL;
  }
  if (sys->seek(this->d->infh, file->section->chm->sec0.offset
		+ file->offset, MSPACK_SYS_SEEK_START))
  {
    this->error = MSPACK_ERR_SEEK;
    sys->free(data);
    return NULL;
  }
  if (sys->read(this->d->infh, data, len) != len) {
    this->error = MSPACK_ERR_READ;
    sys->free(data);
    return NULL;
  }
  return data;
}

/***************************************
 * CHMD_ERROR
 ***************************************
 * returns the last error that occurred
 */
static int chmd_error(struct mschm_decompressor *base) {
  struct mschm_decompressor_p *this = (struct mschm_decompressor_p *) base;
  return (this) ? this->error : MSPACK_ERR_ARGS;
}
