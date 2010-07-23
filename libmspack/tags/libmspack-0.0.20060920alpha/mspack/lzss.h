/* This file is part of libmspack.
 * (C) 2003-2004 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

#ifndef MSPACK_LZSS_H
#define MSPACK_LZSS_H 1

/* LZSS compression / decompression definitions */

#define LZSS_WINDOW_SIZE (4096)
#define LZSS_WINDOW_FILL (0x20)
#define LZSS_MIN_MATCH   (3)
#define LZSS_MATCH_RANGE (16)

/* SZDD/KWAJ and MS Help have slightly different encodings for the control
 * byte and matches. */
#define LZSS_MODE_EXPAND  (0)
#define LZSS_MODE_MSHELP  (1)

struct lzssd_stream {
  unsigned char window[LZSS_WINDOW_SIZE], mode;
  unsigned char *inbuf, *outbuf, *i_ptr, *o_ptr;
  unsigned int insize, outsize;
};

/**
 * Allocates and initialises LZSS decompression state for decoding an LZSS
 * stream.
 *
 * This routine uses system->alloc() to allocate memory. If memory
 * allocation fails, or the parameters to this function are invalid, NULL
 * is returned.
 *
 * @param system             an mspack_system structure used to read from
 *                           the input stream and write to the output
 *                           stream, also to allocate and free memory.
 * @param input              an input stream with the LZSS data.
 * @param output             an output stream to write the decoded data to.
 * @param mode               either #LZSS_MODE_EXPAND or #LZSS_MODE_MSHELP
 * @return a pointer to a new lzssd_stream, or NULL if there was an error
 */
extern struct lzssd_stream lzssd_init(struct mspack_system *system,
				      struct mspack_file *input,
				      struct mspack_file *output,
				      int mode);

/**
 * Decompresses an entire or partial LZSS stream.
 *
 * The number of bytes of data that should be decompressed is given as the
 * out_bytes parameter. If more bytes are decoded than are needed, they
 * will be kept over for a later invocation.
 *
 * The output bytes will be passed to the system->write() function given
 * in lzssd_init(), using the output file handle given in
 * lzssd_init(). More than one call may be made to system->write().

 * Input bytes will be read in as necessary using the system->read()
 * function given in lzssd_init(), using the input file handle given in
 * lzssd_init().  This will continue until system->read() returns 0 bytes,
 * or an error. Errors will be passed out of the function as
 * MSPACK_ERR_READ errors.  Input streams should convey an "end of input
 * stream" by refusing to supply all the bytes that LZSS asks for when
 * they reach the end of the stream, rather than return an error code.
 *
 * If any error code other than MSPACK_ERR_OK is returned, the stream
 * should be considered unusable and lzssd_decompress() should not be
 * called again on this stream.
 *
 * @param lzss      LZSS decompression state, as allocated by lzssd_init().
 * @param out_bytes the number of bytes of data to decompress.
 * @return an error code, or MSPACK_ERR_OK if successful
 */
extern int lzssd_decompress(struct lzssd_stream *lzssd);

/**
 * Frees all state associated with an LZX data stream. This will call
 * system->free() using the system pointer given in lzxd_init().
 *
 * @param lzx LZX decompression state to free.
 */
extern void lzssd_free(struct lzssd_stream *lzssd);

#endif
