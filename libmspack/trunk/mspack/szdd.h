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

/* SZDD compression definitions */

struct msszdd_compressor_p {
  struct msszdd_compressor base;
  struct mspack_system *system;
  /* todo */
};

/* SZDD decompression definitions */

struct msszdd_decompressor_p {
  struct msszdd_decompressor base;
  struct mspack_system *system;
  /* todo */
};

#endif
