/* This file is part of libmspack.
 * (C) 2003 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

#ifndef MSPACK_KWAJ_H
#define MSPACK_KWAJ_H 1

#include "lzss.h"
#include "mszip.h"

/* generic KWAJ definitions */

/* KWAJ compression definitions */

struct mskwaj_compressor_p {
  struct mskwaj_compressor base;
  struct mspack_system *system;
  /* todo */
};

/* KWAJ decompression definitions */

struct mskwaj_decompressor_p {
  struct mskwaj_decompressor base;
  struct mspack_system *system;
  /* todo */
};

#endif
