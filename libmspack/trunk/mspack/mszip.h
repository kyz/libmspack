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

#ifndef MSPACK_MSZIP_H
#define MSPACK_MSZIP_H 1

/* MSZIP (deflate) compression / (inflate) decompression definitions */

#define MSZIP_FRAME_SIZE  (32768)       /* window size                       */
#define MSZIPD_LBITS      (9)           /* bits in literal/length lookup tbl */
#define MSZIPD_DBITS	  (6)           /* bits in distance lookup table     */
#define MSZIPD_BMAX       (16)          /* maximum bit length of any code    */
#define MSZIPD_NMAX       (288)         /* maximum codes in any set          */

/* huffman table entry */
struct mszipd_huft {
  unsigned char e;                      /* # of extra bits or operation      */
  unsigned char b;                      /* # of bits in this code or subcode */
  union {
    unsigned short n;                   /* literal or length/distance base   */
    struct mszipd_huft *t;              /* pointer to next level of table    */
  } v;
};

struct mszipd_stream {
  struct mspack_system *sys;            /* I/O routines                      */
  struct mspack_file   *input;          /* input file handle                 */
  struct mspack_file   *output;         /* output file handle                */
  unsigned int window_posn;             /* offset within window              */

  unsigned int ll[288+32];              /* code lengths                      */
  int lx[MSZIPD_BMAX+1];                /* memory for l[-1..MSZIPD_BMAX-1]   */
  struct mszipd_huft *u[MSZIPD_BMAX];   /* table stack                       */
  unsigned int x[MSZIPD_BMAX+1];        /* bit offsets, then code stack      */
  unsigned int v[MSZIPD_NMAX];          /* values in order of bit length     */

  /* I/O buffering */
  unsigned char *inbuf, *i_ptr, *i_end, *o_ptr, *o_end;
  unsigned int bit_buffer, bits_left, inbuf_size;

  int error, repair_mode;

  /* 32kb decoding window */
  unsigned char window[MSZIP_FRAME_SIZE];
};

/* allocates MS-ZIP decompression stream for decoding the given stream.
 *
 * - uses system->alloc() to allocate memory
 *
 * - returns NULL if not enough memory
 *
 * - input_buffer_size is how many bytes to use as an input bitstream buffer
 *
 * - if repair_mode is non-zero, errors in decompression will be skipped
 *   and 'holes' left will be filled with zero bytes. This allows at least
 *   a partial recovery of erroneous data.
 */
extern struct mszipd_stream *mszipd_init(struct mspack_system *system,
					struct mspack_file *input,
					struct mspack_file *output,
					int input_buffer_size,
					int repair_mode);

/* decompresses, or decompresses more of, an MS-ZIP stream.
 *
 * - out_bytes of data will be decompressed and the function will return
 *   with an MSPACK_ERR_OK return code.
 *
 * - decompressing will stop as soon as out_bytes is reached. if the true
 *   amount of bytes decoded spills over that amount, they will be kept for
 *   a later invocation of mszipd_decompress().
 *
 * - the output bytes will be passed to the system->write() function given in
 *   mszipd_init(), using the output file handle given in mszipd_init(). More
 *   than one call may be made to system->write()
 *
 * - MS-ZIP will read input bytes as necessary using the system->read()
 *   function given in mszipd_init(), using the input file handle given in
 *   mszipd_init(). This will continue until system->read() returns 0 bytes,
 *   or an error.
 */
extern int mszipd_decompress(struct mszipd_stream *zip, off_t out_bytes);

/* frees all stream associated with an MS-ZIP data stream
 *
 * - calls system->free() using the system pointer given in mszipd_init()
 */
void mszipd_free(struct mszipd_stream *zip);

#endif
