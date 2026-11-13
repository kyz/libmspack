/* This file is part of libmspack.
 * (C) 2003-2004 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

#ifndef MSPACK_HLP_H
#define MSPACK_HLP_H 1

#include <lzss.h>

/* generic HLP definitions */

/* HLP compression definitions */

struct mshlp_compressor_p {
  struct mshlp_compressor base;
  struct mspack_system *system;
  /* todo */
};

/* HLP decompression definitions */

struct mshlpd_btreeheader {
	unsigned short flags;
	unsigned short page_size;
	unsigned char structure[16];
	short page_splits;
	short root_page;
	short total_pages;
	short n_levels;
	int total_entries;
};

struct mshlpd_fileheader {
	off_t reserved, used;
};

struct mshlpd_header_p {
	struct mshlpd_header base;
	
	off_t directory_start;
	off_t free_list_start;
	struct mshlpd_fileheader directory_fh;
	struct mshlpd_btreeheader directory_bh;
	off_t first_page_offset;
	void *page_buffer;
	off_t page_buffer_sz;
};

struct mshlp_decompressor_p {
  struct mshlp_decompressor base;
  struct mspack_system *system;
  int error;
};

#endif
