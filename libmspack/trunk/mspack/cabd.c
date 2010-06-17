/* This file is part of libmspack.
 * (C) 2003-2004 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

/* Cabinet (.CAB) files are a form of file archive. Each cabinet contains
 * "folders", which are compressed spans of data. Each cabinet has
 * "files", whose metadata is in the cabinet header, but whose actual data
 * is stored compressed in one of the "folders". Cabinets can span more
 * than one physical file on disk, in which case they are a "cabinet set",
 * and usually the last folder of each cabinet extends into the next
 * cabinet.
 *
 * For a complete description of the format, get the official Microsoft
 * CAB SDK. It can be found at the following URL:
 *
 *   http://msdn.microsoft.com/library/en-us/dncabsdk/html/cabdl.asp
 *
 * It is a self-extracting ZIP file, which can be extracted with the unzip
 * command.
 */

/* CAB decompression implementation */

#include <system.h>
#include <cab.h>
#include <assert.h>

/* Notes on compliance with cabinet specification:
 *
 * One of the main changes between cabextract 0.6 and libmspack's cab
 * decompressor is the move from block-oriented decompression to
 * stream-oriented decompression.
 *
 * cabextract would read one data block from disk, decompress it with the
 * appropriate method, then write the decompressed data. The CAB
 * specification is specifically designed to work like this, as it ensures
 * compression matches do not span the maximum decompressed block size
 * limit of 32kb.
 *
 * However, the compression algorithms used are stream oriented, with
 * specific hacks added to them to enforce the "individual 32kb blocks"
 * rule in CABs. In other file formats, they do not have this limitation.
 *
 * In order to make more generalised decompressors, libmspack's CAB
 * decompressor has moved from being block-oriented to more stream
 * oriented. This also makes decompression slightly faster.
 *
 * However, this leads to incompliance with the CAB specification. The
 * CAB controller can no longer ensure each block of input given to the
 * decompressors is matched with their output. The "decompressed size" of
 * each individual block is thrown away.
 *
 * Each CAB block is supposed to be seen as individually compressed. This
 * means each consecutive data block can have completely different
 * "uncompressed" sizes, ranging from 1 to 32768 bytes. However, in
 * reality, all data blocks in a folder decompress to exactly 32768 bytes,
 * excepting the final block. 
 *
 * Given this situation, the decompression algorithms are designed to
 * realign their input bitstreams on 32768 output-byte boundaries, and
 * various other special cases have been made. libmspack will not
 * correctly decompress LZX or Quantum compressed folders where the blocks
 * do not follow this "32768 bytes until last block" pattern. It could be
 * implemented if needed, but hopefully this is not necessary -- it has
 * not been seen in over 3Gb of CAB archives.
 */

/* prototypes */
static struct mscabd_cabinet * cabd_open(
  struct mscab_decompressor *base, char *filename);
static void cabd_close(
  struct mscab_decompressor *base, struct mscabd_cabinet *origcab);
static int cabd_read_headers(
  struct mspack_system *sys, struct mspack_file *fh,
  struct mscabd_cabinet_p *cab, off_t offset, int quiet);
static char *cabd_read_string(
  struct mspack_system *sys, struct mspack_file *fh,
  struct mscabd_cabinet_p *cab, int *error);

static struct mscabd_cabinet *cabd_search(
  struct mscab_decompressor *base, char *filename);
static int cabd_find(
  struct mscab_decompressor_p *this, unsigned char *buf,
  struct mspack_file *fh, char *filename, off_t flen,
  off_t *firstlen, struct mscabd_cabinet_p **firstcab);

static int cabd_prepend(
  struct mscab_decompressor *base, struct mscabd_cabinet *cab,
  struct mscabd_cabinet *prevcab);
static int cabd_append(
  struct mscab_decompressor *base, struct mscabd_cabinet *cab,
  struct mscabd_cabinet *nextcab);
static int cabd_merge(
  struct mscab_decompressor *base, struct mscabd_cabinet *lcab,
  struct mscabd_cabinet *rcab);

static int cabd_extract(
  struct mscab_decompressor *base, struct mscabd_file *file, char *filename);
static int cabd_init_decomp(
  struct mscab_decompressor_p *this, unsigned int ct);
static void cabd_free_decomp(
  struct mscab_decompressor_p *this);
static int cabd_sys_read(
  struct mspack_file *file, void *buffer, int bytes);
static int cabd_sys_write(
  struct mspack_file *file, void *buffer, int bytes);
static int cabd_sys_read_block(
  struct mspack_system *sys, struct mscabd_decompress_state *d, int *out,
  int ignore_cksum);
static unsigned int cabd_checksum(
  unsigned char *data, unsigned int bytes, unsigned int cksum);
static struct noned_state *noned_init(
  struct mspack_system *sys, struct mspack_file *in, struct mspack_file *out,
  int bufsize);

static int noned_decompress(
  struct noned_state *s, off_t bytes);
static void noned_free(
  struct noned_state *state);

static int cabd_param(
  struct mscab_decompressor *base, int param, int value);

static int cabd_error(
  struct mscab_decompressor *base);


/***************************************
 * MSPACK_CREATE_CAB_DECOMPRESSOR
 ***************************************
 * constructor
 */
struct mscab_decompressor *
  mspack_create_cab_decompressor(struct mspack_system *sys)
{
  struct mscab_decompressor_p *this = NULL;

  if (!sys) sys = mspack_default_system;
  if (!mspack_valid_system(sys)) return NULL;

  if ((this = sys->alloc(sys, sizeof(struct mscab_decompressor_p)))) {
    this->base.open       = &cabd_open;
    this->base.close      = &cabd_close;
    this->base.search     = &cabd_search;
    this->base.extract    = &cabd_extract;
    this->base.prepend    = &cabd_prepend;
    this->base.append     = &cabd_append;
    this->base.set_param  = &cabd_param;
    this->base.last_error = &cabd_error;
    this->system          = sys;
    this->d               = NULL;
    this->error           = MSPACK_ERR_OK;

    this->param[MSCABD_PARAM_SEARCHBUF] = 32768;
    this->param[MSCABD_PARAM_FIXMSZIP]  = 0;
    this->param[MSCABD_PARAM_DECOMPBUF] = 4096;
  }
  return (struct mscab_decompressor *) this;
}

/***************************************
 * MSPACK_DESTROY_CAB_DECOMPRESSOR
 ***************************************
 * destructor
 */
void mspack_destroy_cab_decompressor(struct mscab_decompressor *base) {
  struct mscab_decompressor_p *this = (struct mscab_decompressor_p *) base;
  if (this) {
    struct mspack_system *sys = this->system;
    cabd_free_decomp(this);
    if (this->d) {
      if (this->d->infh) sys->close(this->d->infh);
      sys->free(this->d);
    }
    sys->free(this);
  }
}


/***************************************
 * CABD_OPEN
 ***************************************
 * opens a file and tries to read it as a cabinet file
 */
static struct mscabd_cabinet *cabd_open(struct mscab_decompressor *base,
					char *filename)
{
  struct mscab_decompressor_p *this = (struct mscab_decompressor_p *) base;
  struct mscabd_cabinet_p *cab = NULL;
  struct mspack_system *sys;
  struct mspack_file *fh;
  int error;

  if (!base) return NULL;
  sys = this->system;

  if ((fh = sys->open(sys, filename, MSPACK_SYS_OPEN_READ))) {
    if ((cab = sys->alloc(sys, sizeof(struct mscabd_cabinet_p)))) {
      cab->base.filename = filename;
      error = cabd_read_headers(sys, fh, cab, (off_t) 0, 0);
      if (error) {
	cabd_close(base, (struct mscabd_cabinet *) cab);
	cab = NULL;
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
  return (struct mscabd_cabinet *) cab;
}

/***************************************
 * CABD_CLOSE
 ***************************************
 * frees all memory associated with a given mscabd_cabinet.
 */
static void cabd_close(struct mscab_decompressor *base,
		       struct mscabd_cabinet *origcab)
{
  struct mscab_decompressor_p *this = (struct mscab_decompressor_p *) base;
  struct mscabd_folder_data *dat, *ndat;
  struct mscabd_cabinet *cab, *ncab;
  struct mscabd_folder *fol, *nfol;
  struct mscabd_file *fi, *nfi;
  struct mspack_system *sys;

  if (!base) return;
  sys = this->system;

  this->error = MSPACK_ERR_OK;

  while (origcab) {
    /* free files */
    for (fi = origcab->files; fi; fi = nfi) {
      nfi = fi->next;
      sys->free(fi->filename);
      sys->free(fi);
    }

    /* free folders */
    for (fol = origcab->folders; fol; fol = nfol) {
      nfol = fol->next;

      /* free folder decompression state if it has been decompressed */
      if (this->d && (this->d->folder == (struct mscabd_folder_p *) fol)) {
	if (this->d->infh) sys->close(this->d->infh);
	cabd_free_decomp(this);
	sys->free(this->d);
	this->d = NULL;
      }

      /* free folder data segments */
      for (dat = ((struct mscabd_folder_p *)fol)->data.next; dat; dat = ndat) {
	ndat = dat->next;
	sys->free(dat);
      }
      sys->free(fol);
    }

    /* free predecessor cabinets (and the original cabinet's strings) */
    for (cab = origcab; cab; cab = ncab) {
      ncab = cab->prevcab;
      sys->free(cab->prevname);
      sys->free(cab->nextname);
      sys->free(cab->previnfo);
      sys->free(cab->nextinfo);
      if (cab != origcab) sys->free(cab);
    }

    /* free successor cabinets */
    for (cab = origcab->nextcab; cab; cab = ncab) {
      ncab = cab->nextcab;
      sys->free(cab->prevname);
      sys->free(cab->nextname);
      sys->free(cab->previnfo);
      sys->free(cab->nextinfo);
      sys->free(cab);
    }

    /* free actual cabinet structure */
    cab = origcab->next;
    sys->free(origcab);

    /* repeat full procedure again with the cab->next pointer (if set) */
    origcab = cab;
  }
}

/***************************************
 * CABD_READ_HEADERS
 ***************************************
 * reads the cabinet file header, folder list and file list.
 * fills out a pre-existing mscabd_cabinet structure, allocates memory
 * for folders and files as necessary
 */
static int cabd_read_headers(struct mspack_system *sys,
			     struct mspack_file *fh,
			     struct mscabd_cabinet_p *cab,
			     off_t offset, int quiet)
{
  int num_folders, num_files, folder_resv, i, x;
  struct mscabd_folder_p *fol, *linkfol = NULL;
  struct mscabd_file *file, *linkfile = NULL;
  unsigned char buf[64];

  /* initialise pointers */
  cab->base.next     = NULL;
  cab->base.files    = NULL;
  cab->base.folders  = NULL;
  cab->base.prevcab  = cab->base.nextcab  = NULL;
  cab->base.prevname = cab->base.nextname = NULL;
  cab->base.previnfo = cab->base.nextinfo = NULL;

  cab->base.base_offset = offset;

  /* seek to CFHEADER */
  if (sys->seek(fh, offset, MSPACK_SYS_SEEK_START)) {
    return MSPACK_ERR_SEEK;
  }

  /* read in the CFHEADER */
  if (sys->read(fh, &buf[0], cfhead_SIZEOF) != cfhead_SIZEOF) {
    return MSPACK_ERR_READ;
  }

  /* check for "MSCF" signature */
  if (EndGetI32(&buf[cfhead_Signature]) != 0x4643534D) {
    return MSPACK_ERR_SIGNATURE;
  }

  /* some basic header fields */
  cab->base.length    = EndGetI32(&buf[cfhead_CabinetSize]);
  cab->base.set_id    = EndGetI16(&buf[cfhead_SetID]);
  cab->base.set_index = EndGetI16(&buf[cfhead_CabinetIndex]);

  /* get the number of folders */
  num_folders = EndGetI16(&buf[cfhead_NumFolders]);
  if (num_folders == 0) {
    if (!quiet) sys->message(fh, "no folders in cabinet.");
    return MSPACK_ERR_DATAFORMAT;
  }

  /* get the number of files */
  num_files = EndGetI16(&buf[cfhead_NumFiles]);
  if (num_files == 0) {
    if (!quiet) sys->message(fh, "no files in cabinet.");
    return MSPACK_ERR_DATAFORMAT;
  }

  /* check cabinet version */
  if ((buf[cfhead_MajorVersion] != 1) && (buf[cfhead_MinorVersion] != 3)) {
    if (!quiet) sys->message(fh, "WARNING; cabinet version is not 1.3");
  }

  /* read the reserved-sizes part of header, if present */
  cab->base.flags = EndGetI16(&buf[cfhead_Flags]);
  if (cab->base.flags & cfheadRESERVE_PRESENT) {
    if (sys->read(fh, &buf[0], cfheadext_SIZEOF) != cfheadext_SIZEOF) {
      return MSPACK_ERR_READ;
    }
    cab->base.header_resv = EndGetI16(&buf[cfheadext_HeaderReserved]);
    folder_resv           = buf[cfheadext_FolderReserved];
    cab->block_resv       = buf[cfheadext_DataReserved];

    if (cab->base.header_resv > 60000) {
      if (!quiet) sys->message(fh, "WARNING; reserved header > 60000.");
    }

    /* skip the reserved header */
    if (cab->base.header_resv) {
      if (sys->seek(fh, (off_t) cab->base.header_resv, MSPACK_SYS_SEEK_CUR)) {
	return MSPACK_ERR_SEEK;
      }
    }
  }
  else {
    cab->base.header_resv = 0;
    folder_resv           = 0; 
    cab->block_resv       = 0;
  }

  /* read name and info of preceeding cabinet in set, if present */
  if (cab->base.flags & cfheadPREV_CABINET) {
    cab->base.prevname = cabd_read_string(sys, fh, cab, &x); if (x) return x;
    cab->base.previnfo = cabd_read_string(sys, fh, cab, &x); if (x) return x;
  }

  /* read name and info of next cabinet in set, if present */
  if (cab->base.flags & cfheadNEXT_CABINET) {
    cab->base.nextname = cabd_read_string(sys, fh, cab, &x); if (x) return x;
    cab->base.nextinfo = cabd_read_string(sys, fh, cab, &x); if (x) return x;
  }

  /* read folders */
  for (i = 0; i < num_folders; i++) {
    if (sys->read(fh, &buf[0], cffold_SIZEOF) != cffold_SIZEOF) {
      return MSPACK_ERR_READ;
    }
    if (folder_resv) {
      if (sys->seek(fh, (off_t) folder_resv, MSPACK_SYS_SEEK_CUR)) {
	return MSPACK_ERR_SEEK;
      }
    }

    if (!(fol = sys->alloc(sys, sizeof(struct mscabd_folder_p)))) {
      return MSPACK_ERR_NOMEMORY;
    }
    fol->base.next       = NULL;
    fol->base.comp_type  = EndGetI16(&buf[cffold_CompType]);
    fol->base.num_blocks = EndGetI16(&buf[cffold_NumBlocks]);
    fol->data.next       = NULL;
    fol->data.cab        = (struct mscabd_cabinet_p *) cab;
    fol->data.offset     = offset + (off_t)
      ( (unsigned int) EndGetI32(&buf[cffold_DataOffset]) );
    fol->merge_prev      = NULL;
    fol->merge_next      = NULL;

    /* link folder into list of folders */
    if (!linkfol) cab->base.folders = (struct mscabd_folder *) fol;
    else linkfol->base.next = (struct mscabd_folder *) fol;
    linkfol = fol;
  }

  /* read files */
  for (i = 0; i < num_files; i++) {
    if (sys->read(fh, &buf[0], cffile_SIZEOF) != cffile_SIZEOF) {
      return MSPACK_ERR_READ;
    }

    if (!(file = sys->alloc(sys, sizeof(struct mscabd_file)))) {
      return MSPACK_ERR_NOMEMORY;
    }

    file->next     = NULL;
    file->length   = EndGetI32(&buf[cffile_UncompressedSize]);
    file->attribs  = EndGetI16(&buf[cffile_Attribs]);
    file->offset   = EndGetI32(&buf[cffile_FolderOffset]);

    /* set folder pointer */
    x = EndGetI16(&buf[cffile_FolderIndex]);
    if (x < cffileCONTINUED_FROM_PREV) {
      /* normal folder index; count up to the correct folder. the folder
       * pointer will be NULL if folder index is invalid */
      struct mscabd_folder *ifol = cab->base.folders; 
      while (x--) if (ifol) ifol = ifol->next;
      file->folder = ifol;

      if (!ifol) {
	sys->free(file);
	D(("invalid folder index"))
	return MSPACK_ERR_DATAFORMAT;
      }
    }
    else {
      /* either CONTINUED_TO_NEXT, CONTINUED_FROM_PREV or
       * CONTINUED_PREV_AND_NEXT */
      if ((x == cffileCONTINUED_TO_NEXT) ||
	  (x == cffileCONTINUED_PREV_AND_NEXT))
      {
	/* get last folder */
	struct mscabd_folder *ifol = cab->base.folders;
	while (ifol->next) ifol = ifol->next;
	file->folder = ifol;

	/* set "merge next" pointer */
	fol = (struct mscabd_folder_p *) ifol;
	if (!fol->merge_next) fol->merge_next = file;
      }

      if ((x == cffileCONTINUED_FROM_PREV) ||
	  (x == cffileCONTINUED_PREV_AND_NEXT))
      {
	/* get first folder */
	file->folder = cab->base.folders;

	/* set "merge prev" pointer */
	fol = (struct mscabd_folder_p *) file->folder;
	if (!fol->merge_prev) fol->merge_prev = file;
      }
    }

    /* get time */
    x = EndGetI16(&buf[cffile_Time]);
    file->time_h = x >> 11;
    file->time_m = (x >> 5) & 0x3F;
    file->time_s = (x << 1) & 0x3E;

    /* get date */
    x = EndGetI16(&buf[cffile_Date]);
    file->date_d = x & 0x1F;
    file->date_m = (x >> 5) & 0xF;
    file->date_y = (x >> 9) + 1980;

    /* get filename */
    file->filename = cabd_read_string(sys, fh, cab, &x);
    if (x) { 
      sys->free(file);
      return x;
    }

    /* link file entry into file list */
    if (!linkfile) cab->base.files = file;
    else linkfile->next = file;
    linkfile = file;
  }

  return MSPACK_ERR_OK;
}

static char *cabd_read_string(struct mspack_system *sys,
			      struct mspack_file *fh,
			      struct mscabd_cabinet_p *cab, int *error)
{
  off_t base = sys->tell(fh);
  char buf[256], *str;
  unsigned int len, i, ok;

  /* read up to 256 bytes */
  len = sys->read(fh, &buf[0], 256);

  /* search for a null terminator in the buffer */
  for (i = 0, ok = 0; i < len; i++) if (!buf[i]) { ok = 1; break; }
  if (!ok) {
    *error = MSPACK_ERR_DATAFORMAT;
    return NULL;
  }

  len = i + 1;

  /* set the data stream to just after the string and return */
  if (sys->seek(fh, base + (off_t)len, MSPACK_SYS_SEEK_START)) {
    *error = MSPACK_ERR_SEEK;
    return NULL;
  }

  if (!(str = sys->alloc(sys, len))) {
    *error = MSPACK_ERR_NOMEMORY;
    return NULL;
  }

  sys->copy(&buf[0], str, len);
  *error = MSPACK_ERR_OK;
  return str;
}
    
/***************************************
 * CABD_SEARCH, CABD_FIND
 ***************************************
 * cabd_search opens a file, finds its extent, allocates a search buffer,
 * then reads through the whole file looking for possible cabinet headers.
 * if it finds any, it tries to read them as real cabinets. returns a linked
 * list of results
 *
 * cabd_find is the inner loop of cabd_search, to make it easier to
 * break out of the loop and be sure that all resources are freed
 */
static struct mscabd_cabinet *cabd_search(struct mscab_decompressor *base,
					  char *filename)
{
  struct mscab_decompressor_p *this = (struct mscab_decompressor_p *) base;
  struct mscabd_cabinet_p *cab = NULL;
  struct mspack_system *sys;
  unsigned char *search_buf;
  struct mspack_file *fh;
  off_t filelen, firstlen = 0;

  if (!base) return NULL;
  sys = this->system;

  /* allocate a search buffer */
  search_buf = sys->alloc(sys, (size_t) this->param[MSCABD_PARAM_SEARCHBUF]);
  if (!search_buf) {
    this->error = MSPACK_ERR_NOMEMORY;
    return NULL;
  }

  /* open file and get its full file length */
  if ((fh = sys->open(sys, filename, MSPACK_SYS_OPEN_READ))) {
    if (!(this->error = mspack_sys_filelen(sys, fh, &filelen))) {
      this->error = cabd_find(this, search_buf, fh, filename,
			      filelen, &firstlen, &cab);
    }

    /* truncated / extraneous data warning: */
    if (firstlen && (firstlen != filelen) &&
	(!cab || (cab->base.base_offset == 0)))
    {
      if (firstlen < filelen) {
	sys->message(fh, "WARNING; possible %" LD
		     " extra bytes at end of file.",
		     filelen - firstlen);
      }
      else {
	sys->message(fh, "WARNING; file possibly truncated by %" LD " bytes.",
		     firstlen - filelen);
      }
    }
    
    sys->close(fh);
  }
  else {
    this->error = MSPACK_ERR_OPEN;
  }

  /* free the search buffer */
  sys->free(search_buf);

  return (struct mscabd_cabinet *) cab;
}

static int cabd_find(struct mscab_decompressor_p *this, unsigned char *buf,
		     struct mspack_file *fh, char *filename, off_t flen,
		     off_t *firstlen, struct mscabd_cabinet_p **firstcab)
{
  struct mscabd_cabinet_p *cab, *link = NULL;
  off_t caboff, offset, foffset, cablen, length;
  struct mspack_system *sys = this->system;
  unsigned char *p, *pend, state = 0;
  unsigned int cablen_u32, foffset_u32;
  int false_cabs = 0;

  /* search through the full file length */
  for (offset = 0; offset < flen; offset += length) {
    /* search length is either the full length of the search buffer, or the
     * amount of data remaining to the end of the file, whichever is less. */
    length = flen - offset;
    if (length > this->param[MSCABD_PARAM_SEARCHBUF]) {
      length = this->param[MSCABD_PARAM_SEARCHBUF];
    }

    /* fill the search buffer with data from disk */
    if (sys->read(fh, &buf[0], (int) length) != (int) length) {
      return MSPACK_ERR_READ;
    }

    /* FAQ avoidance strategy */
    if ((offset == 0) && (EndGetI32(&buf[0]) == 0x28635349)) {
      sys->message(fh, "WARNING; found InstallShield header. "
		   "This is probably an InstallShield file. "
		   "Use UNSHIELD (http://synce.sf.net) to unpack it.");
    }

    /* read through the entire buffer. */
    for (p = &buf[0], pend = &buf[length]; p < pend; ) {
      switch (state) {
	/* starting state */
      case 0:
	/* we spend most of our time in this while loop, looking for
	 * a leading 'M' of the 'MSCF' signature */
	while (p < pend && *p != 0x4D) p++;
	/* if we found tht 'M', advance state */
	if (p++ < pend) state = 1;
	break;

      /* verify that the next 3 bytes are 'S', 'C' and 'F' */
      case 1: state = (*p++ == 0x53) ? 2 : 0; break;
      case 2: state = (*p++ == 0x43) ? 3 : 0; break;
      case 3: state = (*p++ == 0x46) ? 4 : 0; break;

      /* we don't care about bytes 4-7 (see default: for action) */

      /* bytes 8-11 are the overall length of the cabinet */
      case 8:  cablen_u32  = *p++;       state++; break;
      case 9:  cablen_u32 |= *p++ << 8;  state++; break;
      case 10: cablen_u32 |= *p++ << 16; state++; break;
      case 11: cablen_u32 |= *p++ << 24; state++; break;

      /* we don't care about bytes 12-15 (see default: for action) */

      /* bytes 16-19 are the offset within the cabinet of the filedata */
      case 16: foffset_u32  = *p++;       state++; break;
      case 17: foffset_u32 |= *p++ << 8;  state++; break;
      case 18: foffset_u32 |= *p++ << 16; state++; break;
      case 19: foffset_u32 |= *p++ << 24;
	/* now we have recieved 20 bytes of potential cab header. work out
	 * the offset in the file of this potential cabinet */
	caboff = offset + (p - &buf[0]) - 20;

	/* should reading cabinet fail, restart search just after 'MSCF' */
	offset = caboff + 4;

	/* if off_t is only 32-bits signed, there will be overflow problems
	 * with cabinets reaching past the 2GB barrier (or just claiming to)
	 */
#ifndef LARGEFILE_SUPPORT
	if (cablen_u32 & ~0x7FFFFFFF) {
	  sys->message(fh, largefile_msg);
	  cablen_u32 = 0x7FFFFFFF;
	}
	if (foffset_u32 & ~0x7FFFFFFF) {
	  sys->message(fh, largefile_msg);
	  foffset_u32 = 0x7FFFFFFF;
	}
#endif
	/* copy the unsigned 32-bit offsets to signed off_t variables */
	foffset = (off_t) foffset_u32;
	cablen  = (off_t) cablen_u32;

	/* capture the "length of cabinet" field if there is a cabinet at
	 * offset 0 in the file, regardless of whether the cabinet can be
	 * read correctly or not */
	if (caboff == 0) *firstlen = cablen;

	/* check that the files offset is less than the alleged length of
	 * the cabinet, and that the offset + the alleged length are
	 * 'roughly' within the end of overall file length */
	if ((foffset < cablen) &&
	    ((caboff + foffset) < (flen + 32)) &&
	    ((caboff + cablen)  < (flen + 32)) )
	{
	  /* likely cabinet found -- try reading it */
	  if (!(cab = sys->alloc(sys, sizeof(struct mscabd_cabinet_p)))) {
	    return MSPACK_ERR_NOMEMORY;
	  }
	  cab->base.filename = filename;
	  if (cabd_read_headers(sys, fh, cab, caboff, 1)) {
	    /* destroy the failed cabinet */
	    cabd_close((struct mscab_decompressor *) this,
		       (struct mscabd_cabinet *) cab);
	    false_cabs++;
	  }
	  else {
	    /* cabinet read correctly! */

	    /* cause the search to restart after this cab's data. */
	    offset = caboff + cablen;
	      
	    /* link the cab into the list */
	    if (!link) *firstcab = cab;
	    else link->base.next = (struct mscabd_cabinet *) cab;
	    link = cab;
	  }
	}

	/* restart search */
	if (offset >= flen) return MSPACK_ERR_OK;
	if (sys->seek(fh, offset, MSPACK_SYS_SEEK_START)) {
	  return MSPACK_ERR_SEEK;
	}
	length = 0;
	p = pend;
	state = 0;
	break;

      /* for bytes 4-7 and 12-15, just advance state/pointer */
      default:
	p++, state++;
      } /* switch(state) */
    } /* for (... p < pend ...) */
  } /* for (... offset < length ...) */

  if (false_cabs) {
    D(("%d false cabinets found", false_cabs))
  }

  return MSPACK_ERR_OK;
}
					     
/***************************************
 * CABD_MERGE, CABD_PREPEND, CABD_APPEND
 ***************************************
 * joins cabinets together, also merges split folders between these two
 * cabinets only. this includes freeing the duplicate folder and file(s)
 * and allocating a further mscabd_folder_data structure to append to the
 * merged folder's data parts list.
 */
static int cabd_prepend(struct mscab_decompressor *base,
			struct mscabd_cabinet *cab,
			struct mscabd_cabinet *prevcab)
{
  return cabd_merge(base, prevcab, cab);
}

static int cabd_append(struct mscab_decompressor *base,
			struct mscabd_cabinet *cab,
			struct mscabd_cabinet *nextcab)
{
  return cabd_merge(base, cab, nextcab);
}

static int cabd_merge(struct mscab_decompressor *base,
		      struct mscabd_cabinet *lcab,
		      struct mscabd_cabinet *rcab)
{
  struct mscab_decompressor_p *this = (struct mscab_decompressor_p *) base;
  struct mscabd_folder_data *data, *ndata;
  struct mscabd_folder_p *lfol, *rfol;
  struct mscabd_file *fi, *rfi, *lfi;
  struct mscabd_cabinet *cab;
  struct mspack_system *sys;

  if (!this) return MSPACK_ERR_ARGS;
  sys = this->system;

  /* basic args check */
  if (!lcab || !rcab || (lcab == rcab)) {
    D(("lcab NULL, rcab NULL or lcab = rcab"))
    return this->error = MSPACK_ERR_ARGS;
  }

  /* check there's not already a cabinet attached */
  if (lcab->nextcab || rcab->prevcab) {
    D(("cabs already joined"))
    return this->error = MSPACK_ERR_ARGS;
  }

  /* do not create circular cabinet chains */
  for (cab = lcab->prevcab; cab; cab = cab->prevcab) {
    if (cab == rcab) {D(("circular!")) return this->error = MSPACK_ERR_ARGS;}
  }
  for (cab = rcab->nextcab; cab; cab = cab->nextcab) {
    if (cab == lcab) {D(("circular!")) return this->error = MSPACK_ERR_ARGS;}
  }

  /* warn about odd set IDs or indices */
  if (lcab->set_id != rcab->set_id) {
    sys->message(NULL, "WARNING; merged cabinets with differing Set IDs.");
  }

  if (lcab->set_index > rcab->set_index) {
    sys->message(NULL, "WARNING; merged cabinets with odd order.");
  }

  /* merging the last folder in lcab with the first folder in rcab */
  lfol = (struct mscabd_folder_p *) lcab->folders;
  rfol = (struct mscabd_folder_p *) rcab->folders;
  while (lfol->base.next) lfol = (struct mscabd_folder_p *) lfol->base.next;

  /* do we need to merge folders? */
  if (!lfol->merge_next && !rfol->merge_prev) {
    /* no, at least one of the folders is not for merging */

    /* attach cabs */
    lcab->nextcab = rcab;
    rcab->prevcab = lcab;

    /* attach folders */
    lfol->base.next = (struct mscabd_folder *) rfol;

    /* attach files */
    fi = lcab->files;
    while (fi->next) fi = fi->next;
    fi->next = rcab->files;
  }
  else {
    /* folder merge required */

    if (!lfol->merge_next) {
      D(("rcab has merge files, lcab doesn't"))
      return this->error = MSPACK_ERR_DATAFORMAT;
    }

    if (!rfol->merge_prev) {
      D(("lcab has merge files, rcab doesn't"))
      return this->error = MSPACK_ERR_DATAFORMAT;
    }

    /* check that both folders use the same compression method/settings */
    if (lfol->base.comp_type != rfol->base.comp_type) {
      D(("compression type mismatch"))
      return this->error = MSPACK_ERR_DATAFORMAT;
    }

    /* for all files in lfol (which is the last folder in whichever cab),
     * compare them to the files from rfol. they should be identical in
     * number and order. to verify this, check the OFFSETS of each file. */
    lfi = lfol->merge_next;
    rfi = rfol->merge_prev;
    while (lfi) {
      if (!rfi || (lfi->offset !=  rfi->offset)) {
	D(("folder merge mismatch"))
	return this->error = MSPACK_ERR_DATAFORMAT;
      }
      lfi = lfi->next;
      rfi = rfi->next;
    }

    /* allocate a new folder data structure */
    if (!(data = sys->alloc(sys, sizeof(struct mscabd_folder_data)))) {
      return this->error = MSPACK_ERR_NOMEMORY;
    }

    /* attach cabs */
    lcab->nextcab = rcab;
    rcab->prevcab = lcab;

    /* append rfol's data to lfol */
    ndata = &lfol->data;
    while (ndata->next) ndata = ndata->next;
    ndata->next = data;
    *data = rfol->data;
    rfol->data.next = NULL;

    /* lfol becomes rfol.
     * NOTE: special case, don't merge if rfol is merge prev and next,
     * rfol->merge_next is going to be deleted, so keep lfol's version
     * instead */
    lfol->base.num_blocks += rfol->base.num_blocks - 1;
    if ((rfol->merge_next == NULL) ||
	(rfol->merge_next->folder != (struct mscabd_folder *) rfol))
    {
      lfol->merge_next = rfol->merge_next;
    }

    /* attach the rfol's folder (except the merge folder) */
    while (lfol->base.next) lfol = (struct mscabd_folder_p *) lfol->base.next;
    lfol->base.next = rfol->base.next;

    /* free disused merge folder */
    sys->free(rfol);

    /* attach rfol's files */
    fi = lcab->files;
    while (fi->next) fi = fi->next;
    fi->next = rcab->files;

    /* delete all files from rfol's merge folder */
    lfi = NULL;
    for (fi = lcab->files; fi ; fi = rfi) {
      rfi = fi->next;
      /* if file's folder matches the merge folder, unlink and free it */
      if (fi->folder == (struct mscabd_folder *) rfol) {
	if (lfi) lfi->next = rfi; else lcab->files = rfi;
	sys->free(fi->filename);
	sys->free(fi);
      }
      else lfi = fi;
    }
  }

  /* all done! fix files and folders pointers in all cabs so they all
   * point to the same list  */
  for (cab = lcab->prevcab; cab; cab = cab->prevcab) {
    cab->files   = lcab->files;
    cab->folders = lcab->folders;
  }

  for (cab = lcab->nextcab; cab; cab = cab->nextcab) {
    cab->files   = lcab->files;
    cab->folders = lcab->folders;
  }

  return this->error = MSPACK_ERR_OK;
}

/***************************************
 * CABD_EXTRACT
 ***************************************
 * extracts a file from a cabinet
 */
static int cabd_extract(struct mscab_decompressor *base,
			 struct mscabd_file *file, char *filename)
{
  struct mscab_decompressor_p *this = (struct mscab_decompressor_p *) base;
  struct mscabd_folder_p *fol;
  struct mspack_system *sys;
  struct mspack_file *fh;

  if (!this) return MSPACK_ERR_ARGS;
  if (!file) return this->error = MSPACK_ERR_ARGS;

  sys = this->system;
  fol = (struct mscabd_folder_p *) file->folder;

  /* check if file can be extracted */
  if ((!fol) || (fol->merge_prev) ||
      (((file->offset + file->length) / CAB_BLOCKMAX) > fol->base.num_blocks))
  {
    sys->message(NULL, "ERROR; file \"%s\" cannot be extracted, "
		 "cabinet set is incomplete.", file->filename);
    return this->error = MSPACK_ERR_DATAFORMAT;
  }

  /* allocate generic decompression state */
  if (!this->d) {
    this->d = sys->alloc(sys, sizeof(struct mscabd_decompress_state));
    if (!this->d) return this->error = MSPACK_ERR_NOMEMORY;
    this->d->folder     = NULL;
    this->d->data       = NULL;
    this->d->sys        = *sys;
    this->d->sys.read   = &cabd_sys_read;
    this->d->sys.write  = &cabd_sys_write;
    this->d->state      = NULL;
    this->d->infh       = NULL;
    this->d->incab      = NULL;
  }

  /* do we need to change folder or reset the current folder? */
  if ((this->d->folder != fol) || (this->d->offset > file->offset)) {
    /* do we need to open a new cab file? */
    if (!this->d->infh || (fol->data.cab != this->d->incab)) {
      /* close previous file handle if from a different cab */
      if (this->d->infh) sys->close(this->d->infh);
      this->d->incab = fol->data.cab;
      this->d->infh = sys->open(sys, fol->data.cab->base.filename,
				MSPACK_SYS_OPEN_READ);
      if (!this->d->infh) return this->error = MSPACK_ERR_OPEN;
    }
    /* seek to start of data blocks */
    if (sys->seek(this->d->infh, fol->data.offset, MSPACK_SYS_SEEK_START)) {
      return this->error = MSPACK_ERR_SEEK;
    }

    /* set up decompressor */
    if (cabd_init_decomp(this, (unsigned int) fol->base.comp_type)) {
      return this->error;
    }

    /* initialise new folder state */
    this->d->folder = fol;
    this->d->data   = &fol->data;
    this->d->offset = 0;
    this->d->block  = 0;
    this->d->i_ptr = this->d->i_end = &this->d->input[0];
  }

  /* open file for output */
  if (!(fh = sys->open(sys, filename, MSPACK_SYS_OPEN_WRITE))) {
    return this->error = MSPACK_ERR_OPEN;
  }

  this->error = MSPACK_ERR_OK;

  /* if file has more than 0 bytes */
  if (file->length) {
    off_t bytes;
    int error;
    /* get to correct offset.
     * - use NULL fh to say 'no writing' to cabd_sys_write()
     * - MSPACK_ERR_READ returncode indicates error in cabd_sys_read(),
     *   the real error will already be stored in this->error
     */
    this->d->outfh = NULL;
    if ((bytes = file->offset - this->d->offset)) {
      error = this->d->decompress(this->d->state, bytes);
      if (error != MSPACK_ERR_READ) this->error = error;
    }

    /* if getting to the correct offset was error free, unpack file */
    if (!this->error) {
      this->d->outfh = fh;
      error = this->d->decompress(this->d->state, (off_t) file->length);
      if (error != MSPACK_ERR_READ) this->error = error;
    }
  }

  /* close output file */
  sys->close(fh);
  this->d->outfh = NULL;

  return this->error;
}

/***************************************
 * CABD_INIT_DECOMP, CABD_FREE_DECOMP
 ***************************************
 * cabd_init_decomp initialises decompression state, according to which
 * decompression method was used. relies on this->d->folder being the same
 * as when initialised.
 *
 * cabd_free_decomp frees decompression state, according to which method
 * was used.
 */
static int cabd_init_decomp(struct mscab_decompressor_p *this, unsigned int ct)
{
  struct mspack_file *fh = (struct mspack_file *) this;

  assert(this && this->d);

  /* free any existing decompressor */
  cabd_free_decomp(this);

  this->d->comp_type = ct;

  switch (ct & cffoldCOMPTYPE_MASK) {
  case cffoldCOMPTYPE_NONE:
    this->d->decompress = (int (*)(void *, off_t)) &noned_decompress;
    this->d->state = noned_init(&this->d->sys, fh, fh,
				this->param[MSCABD_PARAM_DECOMPBUF]);
    break;
  case cffoldCOMPTYPE_MSZIP:
    this->d->decompress = (int (*)(void *, off_t)) &mszipd_decompress;
    this->d->state = mszipd_init(&this->d->sys, fh, fh,
				 this->param[MSCABD_PARAM_DECOMPBUF],
				 this->param[MSCABD_PARAM_FIXMSZIP]);
    break;
  case cffoldCOMPTYPE_QUANTUM:
    this->d->decompress = (int (*)(void *, off_t)) &qtmd_decompress;
    this->d->state = qtmd_init(&this->d->sys, fh, fh, (int) (ct >> 8) & 0x1f,
			       this->param[MSCABD_PARAM_DECOMPBUF]);
    break;
  case cffoldCOMPTYPE_LZX:
    this->d->decompress = (int (*)(void *, off_t)) &lzxd_decompress;
    this->d->state = lzxd_init(&this->d->sys, fh, fh, (int) (ct >> 8) & 0x1f, 0,
			       this->param[MSCABD_PARAM_DECOMPBUF], (off_t) 0);
    break;
  default:
    return this->error = MSPACK_ERR_DATAFORMAT;
  }
  return this->error = (this->d->state) ? MSPACK_ERR_OK : MSPACK_ERR_NOMEMORY;
}

static void cabd_free_decomp(struct mscab_decompressor_p *this) {
  if (!this || !this->d || !this->d->folder || !this->d->state) return;

  switch (this->d->comp_type & cffoldCOMPTYPE_MASK) {
  case cffoldCOMPTYPE_NONE:    noned_free(this->d->state);   break;
  case cffoldCOMPTYPE_MSZIP:   mszipd_free(this->d->state);  break;
  case cffoldCOMPTYPE_QUANTUM: qtmd_free(this->d->state);    break;
  case cffoldCOMPTYPE_LZX:     lzxd_free(this->d->state);    break;
  }
  this->d->decompress = NULL;
  this->d->state      = NULL;
}

/***************************************
 * CABD_SYS_READ, CABD_SYS_WRITE
 ***************************************
 * cabd_sys_read is the internal reader function which the decompressors
 * use. will read data blocks (and merge split blocks) from the cabinet
 * and serve the read bytes to the decompressors
 *
 * cabd_sys_write is the internal writer function which the decompressors
 * use. it either writes data to disk (this->d->outfh) with the real
 * sys->write() function, or does nothing with the data when
 * this->d->outfh == NULL. advances this->d->offset
 */
static int cabd_sys_read(struct mspack_file *file, void *buffer, int bytes) {
  struct mscab_decompressor_p *this = (struct mscab_decompressor_p *) file;
  unsigned char *buf = (unsigned char *) buffer;
  struct mspack_system *sys = this->system;
  int avail, todo, outlen, ignore_cksum;

  ignore_cksum = this->param[MSCABD_PARAM_FIXMSZIP] &&
    ((this->d->comp_type & cffoldCOMPTYPE_MASK) == cffoldCOMPTYPE_MSZIP);

  todo = bytes;
  while (todo > 0) {
    avail = this->d->i_end - this->d->i_ptr;

    /* if out of input data, read a new block */
    if (avail) {
      /* copy as many input bytes available as possible */
      if (avail > todo) avail = todo;
      sys->copy(this->d->i_ptr, buf, (size_t) avail);
      this->d->i_ptr += avail;
      buf  += avail;
      todo -= avail;
    }
    else {
      /* out of data, read a new block */

      /* check if we're out of input blocks, advance block counter */
      if (this->d->block++ >= this->d->folder->base.num_blocks) {
	this->error = MSPACK_ERR_DATAFORMAT;
	break;
      }

      /* read a block */
      this->error = cabd_sys_read_block(sys, this->d, &outlen, ignore_cksum);
      if (this->error) return -1;

      /* special Quantum hack -- trailer byte to allow the decompressor
       * to realign itself. CAB Quantum blocks, unlike LZX blocks, can have
       * anything from 0 to 4 trailing null bytes. */
      if ((this->d->comp_type & cffoldCOMPTYPE_MASK)==cffoldCOMPTYPE_QUANTUM) {
	*this->d->i_end++ = 0xFF;
      }

      /* is this the last block? */
      if (this->d->block >= this->d->folder->base.num_blocks) {
	/* last block */
	if ((this->d->comp_type & cffoldCOMPTYPE_MASK) == cffoldCOMPTYPE_LZX) {
	  /* special LZX hack -- on the last block, inform LZX of the
	   * size of the output data stream. */
	  lzxd_set_output_length(this->d->state, (off_t)
				 ((this->d->block-1) * CAB_BLOCKMAX + outlen));
	}
      }
      else {
	/* not the last block */
	if (outlen != CAB_BLOCKMAX) {
	  this->system->message(this->d->infh,
				"WARNING; non-maximal data block");
	}
      }
    } /* if (avail) */
  } /* while (todo > 0) */
  return bytes - todo;
}

static int cabd_sys_write(struct mspack_file *file, void *buffer, int bytes) {
  struct mscab_decompressor_p *this = (struct mscab_decompressor_p *) file;
  this->d->offset += bytes;
  if (this->d->outfh) {
    return this->system->write(this->d->outfh, buffer, bytes);
  }
  return bytes;
}

/***************************************
 * CABD_SYS_READ_BLOCK
 ***************************************
 * reads a whole data block from a cab file. the block may span more than
 * one cab file, if it does then the fragments will be reassembled
 */
static int cabd_sys_read_block(struct mspack_system *sys,
			       struct mscabd_decompress_state *d,
			       int *out, int ignore_cksum)
{
  unsigned char hdr[cfdata_SIZEOF];
  unsigned int cksum;
  int len;

  /* reset the input block pointer and end of block pointer */
  d->i_ptr = d->i_end = &d->input[0];

  do {
    /* read the block header */
    if (sys->read(d->infh, &hdr[0], cfdata_SIZEOF) != cfdata_SIZEOF) {
      return MSPACK_ERR_READ;
    }

    /* skip any reserved block headers */
    if (d->data->cab->block_resv &&
	sys->seek(d->infh, (off_t) d->data->cab->block_resv,
		  MSPACK_SYS_SEEK_CUR))
    {
      return MSPACK_ERR_SEEK;
    }

    /* blocks must not be over CAB_INPUTMAX in size */
    len = EndGetI16(&hdr[cfdata_CompressedSize]);
    if (((d->i_end - d->i_ptr) + len) > CAB_INPUTMAX) {
      D(("block size > CAB_INPUTMAX (%d + %d)", d->i_end - d->i_ptr, len))
      return MSPACK_ERR_DATAFORMAT;
    }

     /* blocks must not expand to more than CAB_BLOCKMAX */
    if (EndGetI16(&hdr[cfdata_UncompressedSize]) > CAB_BLOCKMAX) {
      D(("block size > CAB_BLOCKMAX"))
      return MSPACK_ERR_DATAFORMAT;
    }

    /* read the block data */
    if (sys->read(d->infh, d->i_end, len) != len) {
      return MSPACK_ERR_READ;
    }

    /* perform checksum test on the block (if one is stored) */
    if ((cksum = EndGetI32(&hdr[cfdata_CheckSum]))) {
      unsigned int sum2 = cabd_checksum(d->i_end, (unsigned int) len, 0);
      if (cabd_checksum(&hdr[4], 4, sum2) != cksum) {
	if (!ignore_cksum) return MSPACK_ERR_CHECKSUM;
	sys->message(d->infh, "WARNING; bad block checksum found");
      }
    }

    /* advance end of block pointer to include newly read data */
    d->i_end += len;

    /* uncompressed size == 0 means this block was part of a split block
     * and it continues as the first block of the next cabinet in the set.
     * otherwise, this is the last part of the block, and no more block
     * reading needs to be done.
     */
    /* EXIT POINT OF LOOP -- uncompressed size != 0 */
    if ((*out = EndGetI16(&hdr[cfdata_UncompressedSize]))) {
      return MSPACK_ERR_OK;
    }

    /* otherwise, advance to next cabinet */

    /* close current file handle */
    sys->close(d->infh);
    d->infh = NULL;

    /* advance to next member in the cabinet set */
    if (!(d->data = d->data->next)) {
      D(("ran out of splits in cabinet set"))
      return MSPACK_ERR_DATAFORMAT;
    }

    /* open next cab file */
    d->incab = d->data->cab;
    if (!(d->infh = sys->open(sys, d->incab->base.filename,
			      MSPACK_SYS_OPEN_READ)))
    {
      return MSPACK_ERR_OPEN;
    }

    /* seek to start of data blocks */
    if (sys->seek(d->infh, d->data->offset, MSPACK_SYS_SEEK_START)) {
      return MSPACK_ERR_SEEK;
    }
  } while (1);

  /* not reached */
  return MSPACK_ERR_OK;
}

static unsigned int cabd_checksum(unsigned char *data, unsigned int bytes,
				  unsigned int cksum)
{
  unsigned int len, ul = 0;

  for (len = bytes >> 2; len--; data += 4) {
    cksum ^= ((data[0]) | (data[1]<<8) | (data[2]<<16) | (data[3]<<24));
  }

  switch (bytes & 3) {
  case 3: ul |= *data++ << 16;
  case 2: ul |= *data++ <<  8;
  case 1: ul |= *data;
  }
  cksum ^= ul;

  return cksum;
}

/***************************************
 * NONED_INIT, NONED_DECOMPRESS, NONED_FREE
 ***************************************
 * the "not compressed" method decompressor
 */
struct noned_state {
  struct mspack_system *sys;
  struct mspack_file *i;
  struct mspack_file *o;
  unsigned char *buf;
  int bufsize;
};

static struct noned_state *noned_init(struct mspack_system *sys,
				      struct mspack_file *in,
				      struct mspack_file *out,
				      int bufsize)
{
  struct noned_state *state = sys->alloc(sys, sizeof(struct noned_state));
  unsigned char *buf = sys->alloc(sys, (size_t) bufsize);
  if (state && buf) {
    state->sys     = sys;
    state->i       = in;
    state->o       = out;
    state->buf     = buf;
    state->bufsize = bufsize;
  }
  else {
    sys->free(buf);
    sys->free(state);
    state = NULL;
  }
  return state;
}

static int noned_decompress(struct noned_state *s, off_t bytes) {
  int run;
  while (bytes > 0) {
    run = (bytes > s->bufsize) ? s->bufsize : (int) bytes;
    if (s->sys->read(s->i, &s->buf[0], run) != run) return MSPACK_ERR_READ;
    if (s->sys->write(s->o, &s->buf[0], run) != run) return MSPACK_ERR_WRITE;
    bytes -= run;
  }
  return MSPACK_ERR_OK;
}

static void noned_free(struct noned_state *state) {
  struct mspack_system *sys;
  if (state) {
    sys = state->sys;
    sys->free(state->buf);
    sys->free(state);
  }
}


/***************************************
 * CABD_PARAM
 ***************************************
 * allows a parameter to be set
 */
static int cabd_param(struct mscab_decompressor *base, int param, int value) {
  struct mscab_decompressor_p *this = (struct mscab_decompressor_p *) base;
  if (!this) return MSPACK_ERR_ARGS;

  switch (param) {
  case MSCABD_PARAM_SEARCHBUF:
    if (value < 4) return MSPACK_ERR_ARGS;
    this->param[MSCABD_PARAM_SEARCHBUF] = value;
    break;
  case MSCABD_PARAM_FIXMSZIP:
    this->param[MSCABD_PARAM_FIXMSZIP] = value;
    break;
  case MSCABD_PARAM_DECOMPBUF:
    if (value < 4) return MSPACK_ERR_ARGS;
    this->param[MSCABD_PARAM_DECOMPBUF] = value;
    break;
  default:
    return MSPACK_ERR_ARGS;
  }
  return MSPACK_ERR_OK;
}

/***************************************
 * CABD_ERROR
 ***************************************
 * returns the last error that occurred
 */
static int cabd_error(struct mscab_decompressor *base) {
  struct mscab_decompressor_p *this = (struct mscab_decompressor_p *) base;
  return (this) ? this->error : MSPACK_ERR_ARGS;
}
