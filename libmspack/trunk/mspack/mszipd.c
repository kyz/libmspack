/* This file is part of libmspack.
 * (C) 2003 Stuart Caie.
 *
 * The deflate method was created by Phil Katz. MSZIP is equivalent to the
 * deflate method.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

/* MS-ZIP decompression implementation */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <mspack.h>
#include "system.h"
#include "mszip.h"

/* based on an implementation by Dirk Stoecker, itself derived from the
 * Info-ZIP sources.
 */

struct mszipd_stream *mszipd_init(struct mspack_system *system,
				  struct mspack_file *input,
				  struct mspack_file *output,
				  int input_buffer_size,
				  int repair_mode)
{
  struct mszipd_stream *zip;

  if (!system) return NULL;

  input_buffer_size = (input_buffer_size + 1) & -2;
  if (!input_buffer_size) return NULL;

  /* allocate decompression state */
  if (!(zip = system->alloc(system, sizeof(struct mszipd_stream)))) {
    return NULL;
  }

  /* allocate input buffer */
  zip->inbuf  = system->alloc(system, (size_t) input_buffer_size);
  if (!zip->inbuf) {
    system->free(zip);
    return NULL;
  }

  /* initialise decompression state */
  zip->sys             = system;
  zip->input           = input;
  zip->output          = output;
  zip->inbuf_size      = input_buffer_size;
  zip->error           = MSPACK_ERR_OK;
  zip->repair_mode     = repair_mode;

  zip->i_ptr = zip->i_end = &zip->inbuf[0];
  zip->o_ptr = zip->o_end = &zip->window[0];

  return zip;
}

int mszipd_decompress(struct mszipd_stream *zip, off_t out_bytes) {
  int i;

  /* easy answers */
  if (!zip || (out_bytes < 0)) return MSPACK_ERR_ARGS;
  if (zip->error) return zip->error;

  /* flush out any stored-up bytes before we begin */
  i = zip->o_end - zip->o_ptr;
  if ((off_t) i > out_bytes) i = (int) out_bytes;
  if (i) {
    if (zip->sys->write(zip->output, zip->o_ptr, i) != i) {
      return zip->error = MSPACK_ERR_WRITE;
    }
    zip->o_ptr  += i;
    out_bytes   -= i;
  }
  if (out_bytes == 0) return MSPACK_ERR_OK;


  return MSPACK_ERR_DECRUNCH;

}

void mszipd_free(struct mszipd_stream *zip) {
  struct mspack_system *sys;
  if (zip) {
    sys = zip->sys;
    sys->free(zip->inbuf);
    sys->free(zip);
  }
}
