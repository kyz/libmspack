/* This file is part of libmspack.
 * (C) 2003-2004 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

#ifndef MSPACK_SZDD_H
#define MSPACK_SZDD_H 1

#include <lzss.h>

/* generic SZDD definitions */
#define szddhead_Signature  (0x00)
#define szddhead_CompType   (0x08)
#define szddhead_FileChar   (0x09)
#define szddhead_FileLength (0x0A)
#define szddhead_SIZEOF     (0x0E)

#define SZDD_COMPTYPE_A     (0x41)

/* SZDD compression definitions */

struct msszdd_compressor_p {
  struct msszdd_compressor base;
  struct mspack_system *system;
  int error;
};

/* SZDD decompression definitions */

struct msszdd_decompressor_p {
  struct msszdd_decompressor base;
  struct mspack_system *system;
  int error;
};

struct msszddd_header_p {
  struct msszddd_header base;
  struct mspack_file *fh;
};

#endif
