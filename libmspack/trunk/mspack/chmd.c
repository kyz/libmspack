/* This file is part of libmspack.
 * (C) 2003-2011 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

/* CHM decompression implementation */

#include <system.h>
#include <chm.h>

/* prototypes */
static struct mschmd_header * chmd_open(
  struct mschm_decompressor *base, const char *filename);
static struct mschmd_header * chmd_fast_open(
  struct mschm_decompressor *base, const char *filename);
static struct mschmd_header *chmd_real_open(
  struct mschm_decompressor *base, const char *filename, int entire);
static void chmd_close(
  struct mschm_decompressor *base, struct mschmd_header *chm);
static int chmd_read_headers(
  struct mspack_system *sys, struct mspack_file *fh,
  struct mschmd_header *chm, int entire);
static int chmd_fast_find(
  struct mschm_decompressor *base, struct mschmd_header *chm,
  const char *filename, struct mschmd_file *f_ptr, int f_size);
static int chmd_extract(
  struct mschm_decompressor *base, struct mschmd_file *file,
  const char *filename);
static int chmd_sys_write(
  struct mspack_file *file, void *buffer, int bytes);
static int chmd_init_decomp(
  struct mschm_decompressor_p *self, struct mschmd_file *file);
static int read_reset_table(
  struct mschm_decompressor_p *self, struct mschmd_sec_mscompressed *sec,
  int entry, off_t *length_ptr, off_t *offset_ptr);
static int read_spaninfo(
  struct mschm_decompressor_p *self, struct mschmd_sec_mscompressed *sec,
  off_t *length_ptr);
static int find_sys_file(
  struct mschm_decompressor_p *self, struct mschmd_sec_mscompressed *sec,
  struct mschmd_file **f_ptr, const char *name);
static unsigned char *read_sys_file(
  struct mschm_decompressor_p *self, struct mschmd_file *file);
static int chmd_error(
  struct mschm_decompressor *base);
static int read_off64(
  off_t *var, unsigned char *mem, struct mspack_system *sys,
  struct mspack_file *fh);

/* filenames of the system files used for decompression.
 * Content and ControlData are essential.
 * ResetTable is preferred, but SpanInfo can be used if not available
 */
static const char *content_name  = "::DataSpace/Storage/MSCompressed/Content";
static const char *control_name  = "::DataSpace/Storage/MSCompressed/ControlData";
static const char *spaninfo_name = "::DataSpace/Storage/MSCompressed/SpanInfo";
static const char *rtable_name   = "::DataSpace/Storage/MSCompressed/Transform/"
  "{7FC28940-9D31-11D0-9B27-00A0C91E9C7C}/InstanceData/ResetTable";

/***************************************
 * MSPACK_CREATE_CHM_DECOMPRESSOR
 ***************************************
 * constructor
 */
struct mschm_decompressor *
  mspack_create_chm_decompressor(struct mspack_system *sys)
{
  struct mschm_decompressor_p *self = NULL;

  if (!sys) sys = mspack_default_system;
  if (!mspack_valid_system(sys)) return NULL;

  if ((self = (struct mschm_decompressor_p *) sys->alloc(sys, sizeof(struct mschm_decompressor_p)))) {
    self->base.open       = &chmd_open;
    self->base.close      = &chmd_close;
    self->base.extract    = &chmd_extract;
    self->base.last_error = &chmd_error;
    self->base.fast_open  = &chmd_fast_open;
    self->base.fast_find  = &chmd_fast_find;
    self->system          = sys;
    self->error           = MSPACK_ERR_OK;
    self->d               = NULL;
  }
  return (struct mschm_decompressor *) self;
}

/***************************************
 * MSPACK_DESTROY_CAB_DECOMPRESSOR
 ***************************************
 * destructor
 */
void mspack_destroy_chm_decompressor(struct mschm_decompressor *base) {
  struct mschm_decompressor_p *self = (struct mschm_decompressor_p *) base;
  if (self) {
    struct mspack_system *sys = self->system;
    if (self->d) {
      if (self->d->infh)  sys->close(self->d->infh);
      if (self->d->state) lzxd_free(self->d->state);
      sys->free(self->d);
    }
    sys->free(self);
  }
}

/***************************************
 * CHMD_OPEN
 ***************************************
 * opens a file and tries to read it as a CHM file.
 * Calls chmd_real_open() with entire=1.
 */
static struct mschmd_header *chmd_open(struct mschm_decompressor *base,
				       const char *filename)
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
					    const char *filename)
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
					    const char *filename, int entire)
{
  struct mschm_decompressor_p *self = (struct mschm_decompressor_p *) base;
  struct mschmd_header *chm = NULL;
  struct mspack_system *sys;
  struct mspack_file *fh;
  int error;

  if (!base) return NULL;
  sys = self->system;

  if ((fh = sys->open(sys, filename, MSPACK_SYS_OPEN_READ))) {
    if ((chm = (struct mschmd_header *) sys->alloc(sys, sizeof(struct mschmd_header)))) {
      chm->filename = filename;
      error = chmd_read_headers(sys, fh, chm, entire);
      if (error) {
	/* if the error is DATAFORMAT, and there are some results, return
	 * partial results with a warning, rather than nothing */
	if (error == MSPACK_ERR_DATAFORMAT && (chm->files || chm->sysfiles)) {
	  sys->message(fh, "WARNING; contents are corrupt");
	  error = MSPACK_ERR_OK;
	}
	else {
	  chmd_close(base, chm);
	  chm = NULL;
	}
      }
      self->error = error;
    }
    else {
      self->error = MSPACK_ERR_NOMEMORY;
    }
    sys->close(fh);
  }
  else {
    self->error = MSPACK_ERR_OPEN;
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
  struct mschm_decompressor_p *self = (struct mschm_decompressor_p *) base;
  struct mschmd_file *fi, *nfi;
  struct mspack_system *sys;

  if (!base) return;
  sys = self->system;

  self->error = MSPACK_ERR_OK;

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
  if (self->d && (self->d->chm == chm)) {
    if (self->d->infh) sys->close(self->d->infh);
    if (self->d->state) lzxd_free(self->d->state);
    sys->free(self->d);
    self->d = NULL;
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
static const unsigned char guids[32] = {
  /* {7C01FD10-7BAA-11D0-9E0C-00A0-C922-E6EC} */
  0x10, 0xFD, 0x01, 0x7C, 0xAA, 0x7B, 0xD0, 0x11,
  0x9E, 0x0C, 0x00, 0xA0, 0xC9, 0x22, 0xE6, 0xEC,
  /* {7C01FD11-7BAA-11D0-9E0C-00A0-C922-E6EC} */
  0x11, 0xFD, 0x01, 0x7C, 0xAA, 0x7B, 0xD0, 0x11,
  0x9E, 0x0C, 0x00, 0xA0, 0xC9, 0x22, 0xE6, 0xEC
};

/* reads an encoded integer into a variable; 7 bits of data per byte,
 * the high bit is used to indicate that there is another byte */
#define READ_ENCINT(var) do {			\
    (var) = 0;					\
    do {					\
	if (p > end) goto chunk_end;		\
	(var) = ((var) << 7) | (*p & 0x7F);	\
    } while (*p++ & 0x80);			\
} while (0)

static int chmd_read_headers(struct mspack_system *sys, struct mspack_file *fh,
			     struct mschmd_header *chm, int entire)
{
  unsigned int section, name_len, x, errors, num_chunks;
  unsigned char buf[0x54], *chunk = NULL, *name, *p, *end;
  struct mschmd_file *fi, *link = NULL;
  off_t offset, length;
  int num_entries;

  /* initialise pointers */
  chm->files         = NULL;
  chm->sysfiles      = NULL;
  chm->sec0.base.chm = chm;
  chm->sec0.base.id  = 0;
  chm->sec1.base.chm = chm;
  chm->sec1.base.id  = 1;
  chm->sec1.content  = NULL;
  chm->sec1.control  = NULL;
  chm->sec1.spaninfo = NULL;
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
  if (mspack_memcmp(&buf[chmhead_GUID1], &guids[0], 32L) != 0) {
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
  if (read_off64(&offset,           &buf[chmhst_OffsetHS0],  sys, fh) ||
      read_off64(&chm->dir_offset,  &buf[chmhst_OffsetHS1],  sys, fh) ||
      read_off64(&chm->sec0.offset, &buf[chmhst3_OffsetCS0], sys, fh))
  {
    return MSPACK_ERR_DATAFORMAT;
  }

  /* seek to header section 0 */
  if (sys->seek(fh, offset, MSPACK_SYS_SEEK_START)) {
    return MSPACK_ERR_SEEK;
  }

  /* read header section 0 */
  if (sys->read(fh, &buf[0], chmhs0_SIZEOF) != chmhs0_SIZEOF) {
    return MSPACK_ERR_READ;
  }
  if (read_off64(&chm->length, &buf[chmhs0_FileLen], sys, fh)) {
    return MSPACK_ERR_DATAFORMAT;
  }

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

  /* ensure chunk size is large enough for signature and num_entries */
  if (chm->chunk_size < (pmgl_Entries + 2)) {
    return MSPACK_ERR_DATAFORMAT;
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

  if (!(chunk = (unsigned char *) sys->alloc(sys, (size_t)chm->chunk_size))) {
    return MSPACK_ERR_NOMEMORY;
  }

  /* read and process all chunks from FirstPMGL to LastPMGL */
  errors = 0;
  while (num_chunks--) {
    /* read next chunk */
    if (sys->read(fh, chunk, (int)chm->chunk_size) != (int)chm->chunk_size) {
      sys->free(chunk);
      return MSPACK_ERR_READ;
    }

    /* process only directory (PMGL) chunks */
    if (EndGetI32(&chunk[pmgl_Signature]) != 0x4C474D50) continue;

    if (EndGetI32(&chunk[pmgl_QuickRefSize]) < 2) {
      sys->message(fh, "WARNING; PMGL quickref area is too small");
    }
    if (EndGetI32(&chunk[pmgl_QuickRefSize]) > 
	((int)chm->chunk_size - pmgl_Entries))
    {
      sys->message(fh, "WARNING; PMGL quickref area is too large");
    }

    p = &chunk[pmgl_Entries];
    end = &chunk[chm->chunk_size - 2];
    num_entries = EndGetI16(end);

    while (num_entries--) {
      READ_ENCINT(name_len); name = p; p += name_len;
      READ_ENCINT(section);
      READ_ENCINT(offset);
      READ_ENCINT(length);

      /* empty files and directory names are stored as a file entry at
       * offset 0 with length 0. We want to keep empty files, but not
       * directory names, which end with a "/" */
      if ((offset == 0) && (length == 0)) {
	if ((name_len > 0) && (name[name_len-1] == '/')) continue;
      }

      if (section > 1) {
	sys->message(fh, "invalid section number '%u'.", section);
	continue;
      }

      if (!(fi = (struct mschmd_file *) sys->alloc(sys, sizeof(struct mschmd_file) + name_len + 1))) {
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
	if (mspack_memcmp(&name[2], &content_name[2], 31L) == 0) {
	  if (mspack_memcmp(&name[33], &content_name[33], 8L) == 0) {
	    chm->sec1.content = fi;
	  }
	  else if (mspack_memcmp(&name[33], &control_name[33], 11L) == 0) {
	    chm->sec1.control = fi;
	  }
	  else if (mspack_memcmp(&name[33], &spaninfo_name[33], 8L) == 0) {
	    chm->sec1.spaninfo = fi;
	  }
	  else if (mspack_memcmp(&name[33], &rtable_name[33], 72L) == 0) {
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

    /* this is reached either when num_entries runs out, or if
     * reading data from the chunk reached a premature end of chunk */
  chunk_end:
    if (num_entries >= 0) {
      D(("chunk ended before all entries could be read"))
      errors++;
    }

  }
  sys->free(chunk);
  return (errors > 0) ? MSPACK_ERR_DATAFORMAT : MSPACK_ERR_OK;
}

/***************************************
 * CABD_FAST_FIND
 ***************************************
 * uses PMGI index chunks and quickref data to quickly locate a file
 * directly from the on-disk index.
 */
static int chmd_fast_find(struct mschm_decompressor *base,
			  struct mschmd_header *chm, const char *filename,
			  struct mschmd_file *f_ptr, int f_size)
{
  struct mschm_decompressor_p *self = (struct mschm_decompressor_p *) base;
  struct mspack_system *sys;
  struct mspack_file *fh;
  unsigned int block;
  unsigned char *chunk;

  if (!self || !chm || !f_ptr || (f_size != sizeof(struct mschmd_file))) {
    return MSPACK_ERR_ARGS;
  }
  sys = self->system;

  if (!(chunk = (unsigned char *) sys->alloc(sys, (size_t)chm->chunk_size))) {
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

  /* for now - no results */
  f_ptr->section = NULL;
  f_ptr->offset = 0;
  f_ptr->length = 0;
  return MSPACK_ERR_OK;
}


/***************************************
 * CABD_EXTRACT
 ***************************************
 * extracts a file from a CHM helpfile
 */
static int chmd_extract(struct mschm_decompressor *base,
			struct mschmd_file *file, const char *filename)
{
  struct mschm_decompressor_p *self = (struct mschm_decompressor_p *) base;
  struct mspack_system *sys;
  struct mschmd_header *chm;
  struct mspack_file *fh;
  off_t bytes;

  if (!self) return MSPACK_ERR_ARGS;
  if (!file || !file->section) return self->error = MSPACK_ERR_ARGS;
  sys = self->system;
  chm = file->section->chm;

  /* create decompression state if it doesn't exist */
  if (!self->d) {
    self->d = (struct mschmd_decompress_state *) sys->alloc(sys, sizeof(struct mschmd_decompress_state));
    if (!self->d) return self->error = MSPACK_ERR_NOMEMORY;
    self->d->chm       = chm;
    self->d->offset    = 0;
    self->d->state     = NULL;
    self->d->sys       = *sys;
    self->d->sys.write = &chmd_sys_write;
    self->d->infh      = NULL;
    self->d->outfh     = NULL;
  }

  /* open input chm file if not open, or the open one is a different chm */
  if (!self->d->infh || (self->d->chm != chm)) {
    if (self->d->infh)  sys->close(self->d->infh);
    if (self->d->state) lzxd_free(self->d->state);
    self->d->chm    = chm;
    self->d->offset = 0;
    self->d->state  = NULL;
    self->d->infh   = sys->open(sys, chm->filename, MSPACK_SYS_OPEN_READ);
    if (!self->d->infh) return self->error = MSPACK_ERR_OPEN;
  }

  /* open file for output */
  if (!(fh = sys->open(sys, filename, MSPACK_SYS_OPEN_WRITE))) {
    return self->error = MSPACK_ERR_OPEN;
  }

  /* if file is empty, simply creating it is enough */
  if (!file->length) {
    sys->close(fh);
    return self->error = MSPACK_ERR_OK;
  }

  self->error = MSPACK_ERR_OK;

  switch (file->section->id) {
  case 0: /* Uncompressed section file */
    /* simple seek + copy */
    if (sys->seek(self->d->infh, file->section->chm->sec0.offset
		  + file->offset, MSPACK_SYS_SEEK_START))
    {
      self->error = MSPACK_ERR_SEEK;
    }
    else {
      unsigned char buf[512];
      off_t length = file->length;
      while (length > 0) {
	int run = sizeof(buf);
	if ((off_t)run > length) run = (int)length;
	if (sys->read(self->d->infh, &buf[0], run) != run) {
	  self->error = MSPACK_ERR_READ;
	  break;
	}
	if (sys->write(fh, &buf[0], run) != run) {
	  self->error = MSPACK_ERR_WRITE;
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
    if (!self->d->state || (file->offset < self->d->offset)) {
      if (self->d->state) {
	lzxd_free(self->d->state);
	self->d->state = NULL;
      }
      if (chmd_init_decomp(self, file)) break;
    }

    /* seek to input data */
    if (sys->seek(self->d->infh, self->d->inoffset, MSPACK_SYS_SEEK_START)) {
      self->error = MSPACK_ERR_SEEK;
      break;
    }

    /* get to correct offset. */
    self->d->outfh = NULL;
    if ((bytes = file->offset - self->d->offset)) {
      self->error = lzxd_decompress(self->d->state, bytes);
    }

    /* if getting to the correct offset was error free, unpack file */
    if (!self->error) {
      self->d->outfh = fh;
      self->error = lzxd_decompress(self->d->state, file->length);
    }

    /* save offset in input source stream, in case there is a section 0
     * file between now and the next section 1 file extracted */
    self->d->inoffset = sys->tell(self->d->infh);

    /* if an LZX error occured, the LZX decompressor is now useless */
    if (self->error) {
      if (self->d->state) lzxd_free(self->d->state);
      self->d->state = NULL;
    }
    break;
  }

  sys->close(fh);
  return self->error;
}

/***************************************
 * CHMD_SYS_WRITE
 ***************************************
 * chmd_sys_write is the internal writer function which the decompressor
 * uses. If either writes data to disk (self->d->outfh) with the real
 * sys->write() function, or does nothing with the data when
 * self->d->outfh == NULL. advances self->d->offset.
 */
static int chmd_sys_write(struct mspack_file *file, void *buffer, int bytes) {
  struct mschm_decompressor_p *self = (struct mschm_decompressor_p *) file;
  self->d->offset += bytes;
  if (self->d->outfh) {
    return self->system->write(self->d->outfh, buffer, bytes);
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
static int chmd_init_decomp(struct mschm_decompressor_p *self,
			    struct mschmd_file *file)
{
  int window_size, window_bits, reset_interval, entry, err;
  struct mspack_system *sys = self->system;
  struct mschmd_sec_mscompressed *sec;
  unsigned char *data;
  off_t length, offset;

  sec = (struct mschmd_sec_mscompressed *) file->section;

  /* ensure we have a mscompressed content section */
  err = find_sys_file(self, sec, &sec->content, content_name);
  if (err) return self->error = err;

  /* ensure we have a ControlData file */
  err = find_sys_file(self, sec, &sec->control, control_name);
  if (err) return self->error = err;

  /* read ControlData */
  if (sec->control->length < lzxcd_SIZEOF) {
    D(("ControlData file is too short"))
    return self->error = MSPACK_ERR_DATAFORMAT;
  }
  if (!(data = read_sys_file(self, sec->control))) {
    D(("can't read mscompressed control data file"))
    return self->error;
  }

  /* check LZXC signature */
  if (EndGetI32(&data[lzxcd_Signature]) != 0x43585A4C) {
    sys->free(data);
    return self->error = MSPACK_ERR_SIGNATURE;
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
    return self->error = MSPACK_ERR_DATAFORMAT;
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
    return self->error = MSPACK_ERR_DATAFORMAT;
  }

  /* validate reset_interval */
  if (reset_interval % LZX_FRAME_SIZE) {
    D(("bad controldata reset interval"))
    return self->error = MSPACK_ERR_DATAFORMAT;
  }

  /* which reset table entry would we like? */
  entry = file->offset / reset_interval;
  /* convert from reset interval multiple (usually 64k) to 32k frames */
  entry *= reset_interval / LZX_FRAME_SIZE;

  /* read the reset table entry */
  if (read_reset_table(self, sec, entry, &length, &offset)) {
    /* the uncompressed length given in the reset table is dishonest.
     * the uncompressed data is always padded out from the given
     * uncompressed length up to the next reset interval */
    length += reset_interval - 1;
    length &= -reset_interval;
  }
  else {
    /* if we can't read the reset table entry, just start from
     * the beginning. Use spaninfo to get the uncompressed length */
    entry = 0;
    offset = 0;
    err = read_spaninfo(self, sec, &length);
  }
  if (err) return self->error = err;

  /* get offset of compressed data stream:
   * = offset of uncompressed section from start of file
   * + offset of compressed stream from start of uncompressed section
   * + offset of chosen reset interval from start of compressed stream */
  self->d->inoffset = file->section->chm->sec0.offset + sec->content->offset + offset;

  /* set start offset and overall remaining stream length */
  self->d->offset = entry * LZX_FRAME_SIZE;
  length -= self->d->offset;

  /* initialise LZX stream */
  self->d->state = lzxd_init(&self->d->sys, self->d->infh,
			     (struct mspack_file *) self, window_bits,
			     reset_interval / LZX_FRAME_SIZE,
			     4096, length);
  if (!self->d->state) self->error = MSPACK_ERR_NOMEMORY;
  return self->error;
}

/***************************************
 * READ_RESET_TABLE
 ***************************************
 * Reads one entry out of the reset table. Also reads the uncompressed
 * data length. Writes these to offset_ptr and length_ptr respectively.
 * Returns non-zero for success, zero for failure.
 */
static int read_reset_table(struct mschm_decompressor_p *self,
			    struct mschmd_sec_mscompressed *sec,
			    int entry, off_t *length_ptr, off_t *offset_ptr)
{
    struct mspack_system *sys = self->system;
    unsigned char *data;
    int pos, entrysize;

    /* do we have a ResetTable file? */
    int err = find_sys_file(self, sec, &sec->rtable, rtable_name);
    if (err) return 0;

    /* read ResetTable file */
    if (sec->rtable->length < lzxrt_headerSIZEOF) {
	D(("ResetTable file is too short"))
	return 0;
    }
    if (!(data = read_sys_file(self, sec->rtable))) {
	D(("can't read reset table"))
	return 0;
    }

    /* check sanity of reset table */
    if (EndGetI32(&data[lzxrt_FrameLen]) != LZX_FRAME_SIZE) {
	D(("bad reset table frame length"))
	sys->free(data);
	return 0;
    }

    /* get the uncompressed length of the LZX stream */
    if (read_off64(length_ptr, data, sys, self->d->infh)) {
	sys->free(data);
	return 0;
    }

    entrysize = EndGetI32(&data[lzxrt_EntrySize]);
    pos = EndGetI32(&data[lzxrt_TableOffset]) + (entry * entrysize);

    /* ensure reset table entry for this offset exists */
    if (entry < EndGetI32(&data[lzxrt_NumEntries]) &&
	((pos + entrysize) <= sec->rtable->length))
    {
	switch (entrysize) {
	case 4:
	    *offset_ptr = EndGetI32(&data[pos]);
	    err = 0;
	    break;
	case 8:
	    err = read_off64(offset_ptr, &data[pos], sys, self->d->infh);
	    break;
	default:
	    D(("reset table entry size neither 4 nor 8"))
	    err = 1;
	    break;
	}
    }
    else {
	D(("bad reset interval"))
	err = 1;
    }

    /* free the reset table */
    sys->free(data);

    /* return success */
    return (err == 0);
}

/***************************************
 * READ_SPANINFO
 ***************************************
 * Reads the uncompressed data length from the spaninfo file.
 * Returns zero for success or a non-zero error code for failure.
 */
static int read_spaninfo(struct mschm_decompressor_p *self,
			 struct mschmd_sec_mscompressed *sec,
			 off_t *length_ptr)
{
    struct mspack_system *sys = self->system;
    unsigned char *data;
    
    /* find SpanInfo file */
    int err = find_sys_file(self, sec, &sec->spaninfo, spaninfo_name);
    if (err) return MSPACK_ERR_DATAFORMAT;

    /* check it's large enough */
    if (sec->spaninfo->length != 8) {
	D(("SpanInfo file is wrong size"))
	return MSPACK_ERR_DATAFORMAT;
    }

    /* read the SpanInfo file */
    if (!(data = read_sys_file(self, sec->spaninfo))) {
	D(("can't read SpanInfo file"))
	return self->error;
    }

    /* get the uncompressed length of the LZX stream */
    err = read_off64(length_ptr, data, sys, self->d->infh);

    sys->free(data);
    return (err) ? MSPACK_ERR_DATAFORMAT : MSPACK_ERR_OK;
}

/***************************************
 * FIND_SYS_FILE
 ***************************************
 * Uses chmd_fast_find to locate a system file, and fills out that system
 * file's entry and links it into the list of system files. Returns zero
 * for success, non-zero for both failure and the file not existing.
 */
static int find_sys_file(struct mschm_decompressor_p *self,
			 struct mschmd_sec_mscompressed *sec,
			 struct mschmd_file **f_ptr, const char *name)
{
    struct mspack_system *sys = self->system;
    struct mschmd_file result;

    /* already loaded */
    if (*f_ptr) return MSPACK_ERR_OK;

    /* try using fast_find to find the file - return DATAFORMAT error if
     * it fails, or successfully doesn't find the file */
    if (chmd_fast_find((struct mschm_decompressor *) self, sec->base.chm,
		       name, &result, (int)sizeof(result)) || !result.section)
    {
	return MSPACK_ERR_DATAFORMAT;
    }

    if (!(*f_ptr = (struct mschmd_file *) sys->alloc(sys, sizeof(result)))) {
	return MSPACK_ERR_NOMEMORY;
    }

    /* copy result */
    *(*f_ptr) = result;
    (*f_ptr)->filename = (char *) name;

    /* link file into sysfiles list */
    (*f_ptr)->next = sec->base.chm->sysfiles;
    sec->base.chm->sysfiles = *f_ptr;
    return MSPACK_ERR_OK;
}

/***************************************
 * READ_SYS_FILE
 ***************************************
 * Allocates memory for a section 0 (uncompressed) file and reads it into
 * memory.
 */
static unsigned char *read_sys_file(struct mschm_decompressor_p *self,
				    struct mschmd_file *file)
{
  struct mspack_system *sys = self->system;
  unsigned char *data = NULL;
  int len;

  if (!file || !file->section || (file->section->id != 0)) {
    self->error = MSPACK_ERR_DATAFORMAT;
    return NULL;
  }

  len = (int) file->length;

  if (!(data = (unsigned char *) sys->alloc(sys, (size_t) len))) {
    self->error = MSPACK_ERR_NOMEMORY;
    return NULL;
  }
  if (sys->seek(self->d->infh, file->section->chm->sec0.offset
		+ file->offset, MSPACK_SYS_SEEK_START))
  {
    self->error = MSPACK_ERR_SEEK;
    sys->free(data);
    return NULL;
  }
  if (sys->read(self->d->infh, data, len) != len) {
    self->error = MSPACK_ERR_READ;
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
  struct mschm_decompressor_p *self = (struct mschm_decompressor_p *) base;
  return (self) ? self->error : MSPACK_ERR_ARGS;
}

/***************************************
 * READ_OFF64
 ***************************************
 * Reads a 64-bit signed integer from memory in Intel byte order.
 * If running on a system with a 64-bit off_t, this is simply done.
 * If running on a system with a 32-bit off_t, offsets up to 0x7FFFFFFF
 * are accepted, offsets beyond that cause an error message.
 */
static int read_off64(off_t *var, unsigned char *mem,
		      struct mspack_system *sys, struct mspack_file *fh)
{
#ifdef LARGEFILE_SUPPORT
    *var = EndGetI64(mem);
#else
    *var = EndGetI32(mem);
    if ((*var & 0x80000000) || EndGetI32(mem+4)) {
	sys->message(fh, (char *)largefile_msg);
	return 1;
    }
#endif
    return 0;
}
