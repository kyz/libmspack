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

/* Tables for deflate from PKZIP's appnote.txt. */

/* Order of the bit length code lengths */
static const unsigned char mszipd_border[] = {
  16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

/* Copy lengths for literal codes 257..285 */
static const unsigned short mszipd_cplens[] = {
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
  59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0
};

/* Extra bits for literal codes 257..285 */
static const unsigned short mszipd_cplext[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
  4, 5, 5, 5, 5, 0, 99, 99 /* 99==invalid */
};

/* Copy offsets for distance codes 0..29 */
static const unsigned short mszipd_cpdist[] = {
  1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
  513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

/* Extra bits for distance codes */
static const unsigned short mszipd_cpdext[] = {
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
  10, 11, 11, 12, 12, 13, 13
};

/* ANDing with mszipd_mask[n] masks the lower n bits */
static const unsigned short mszipd_mask[] = {
 0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
 0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

#define STORE_BITS do {                                                 \
  zip->i_ptr      = i_ptr;                                              \
  zip->i_end      = i_end;                                              \
  zip->bit_buffer = bit_buffer;                                         \
  zip->bits_left  = bits_left;                                          \
} while (0)

#define RESTORE_BITS do {                                               \
  i_ptr      = zip->i_ptr;                                              \
  i_end      = zip->i_end;                                              \
  bit_buffer = zip->bit_buffer;                                         \
  bits_left  = zip->bits_left;                                          \
} while (0)

#define NEED_BITS(nbits) do {                                           \
  while (bits_left < (nbits)) {                                         \
    if (i_ptr >= i_end) {                                               \
      if (zipd_read_input(zip)) return zip->error;                      \
      i_ptr = zip->i_ptr;                                               \
      i_end = zip->i_end;                                               \
    }                                                                   \
    bit_buffer |= *i_ptr++ << bits_left; bits_left  += 8;               \
  }                                                                     \
} while (0)

#define PEEK_BITS(mask) (bit_buffer & mask)

#define DUMP_BITS(nbits) ((bit_buffer >>= (nbits)), (bits_left -= (nbits)))

#define READ_BITS_MASK(val, nbits, mask) do {                           \
  NEED_BITS(nbits); (val) = PEEK_BITS(mask); DUMP_BITS(nbits);          \
} while (0)

#define READ_BITS(val, nbits) READ_BITS_MASK(val, nbits, mszipd_mask[nbits])
#define READ_1BIT(val)        READ_BITS_MASK(val, 1, 0x01)
#define READ_2BITS(val)       READ_BITS_MASK(val, 2, 0x03)
#define READ_3BITS(val)       READ_BITS_MASK(val, 3, 0x07)
#define READ_4BITS(val)       READ_BITS_MASK(val, 4, 0x0F)
#define READ_5BITS(val)       READ_BITS_MASK(val, 5, 0x1F)
#define READ_7BITS(val)       READ_BITS_MASK(val, 7, 0x7F)
#define READ_8BITS(val)       READ_BITS_MASK(val, 8, 0xFF)

static int zipd_read_input(struct mszipd_stream *zip) {
  int read = zip->sys->read(zip->input, &zip->inbuf[0], (int)zip->inbuf_size);
  if (read < 0) return zip->error = MSPACK_ERR_READ;
  zip->i_ptr = &zip->inbuf[0];
  zip->i_end = &zip->inbuf[read];

  return MSPACK_ERR_OK;
}


static void mszipd_huft_free(struct mszipd_stream *zip, struct mszipd_huft *h)
{
  struct mszipd_huft *n;

  /* Go through linked list, freeing from the allocated (h[-1]) address */
  while (h) {
    h--;
    n = h->v.t;
    zip->sys->free(h);
    h = n;
  } 
}

static int mszipd_huft_build(struct mszipd_stream *zip, unsigned int *b,
			     unsigned int n, unsigned int s, unsigned short *d,
			     unsigned short *e, struct mszipd_huft **t, int *m)
{
  unsigned int a;                 /* counter for codes of length k */
  unsigned int c[MSZIPD_BMAX+1];  /* bit length count table */
  unsigned int el;                /* length of EOB code (value 256) */
  unsigned int f;                 /* i repeats in table every f entries */
  int g;                          /* maximum code length */
  int h;                          /* table level */
  register unsigned int i;        /* counter, current code */
  register unsigned int j;        /* counter */
  register int k;                 /* number of bits in current code */
  int *l;                         /* stack of bits per table */
  register unsigned int *p;       /* pointer into c[], b[] and v[] */
  register struct mszipd_huft *q; /* points to current table */
  struct mszipd_huft r;           /* table entry for structure assignment */
  register int w;                 /* bits before this table == (l * h) */
  unsigned int *xp;               /* pointer into x */
  int y;                          /* number of dummy codes added */
  unsigned int z;                 /* number of entries in current table */

  l = &zip->lx[1];

  /* Generate counts for each bit length */
  el = n > 256 ? b[256] : MSZIPD_BMAX; /* set length of EOB code, if any */

  for (i = 0; i <= MSZIPD_BMAX; i++) {
    c[i] = 0;
  }

  /* assume all entries <= MSZIPD_BMAX */
  p = b;  i = n;
  do { c[*p]++; p++;} while (--i);

  /* null input--all zero length codes */
  if (c[0] == n) {
    *t = NULL;
    *m = 0;
    return 0;
  }

  /* Find minimum and maximum length, bound *m by those */
  for (j = 1; j <= MSZIPD_BMAX; j++) {
    if (c[j]) break;
  }
  k = j;                        /* minimum code length */
  if (*m < j) *m = j;

  for (i = MSZIPD_BMAX; i; i--) {
    if (c[i]) break;
  }
  g = i;                        /* maximum code length */
  if (*m > i) *m = i;

  /* Adjust last length count to fill out codes, if needed */
  for (y = 1 << j; j < i; j++, y <<= 1) {
    /* bad input: more codes than bits */
    if ((y -= c[j]) < 0) return 2;
  }
  if ((y -= c[i]) < 0) return 2;
  c[i] += y;

  /* Generate starting offsets into the value table for each length */
  zip->x[1] = j = 0;
  p = c + 1;  xp = &zip->x[2];
  while (--i) *xp++ = (j += *p++);  /* note that i == g from above */

  /* Make a table of values in order of bit lengths */
  p = b;  i = 0;
  do { if ((j = *p++) != 0) zip->v[zip->x[j]++] = i; } while (++i < n);

  /* Generate the Huffman codes and for each, make the table entries */
  zip->x[0] = i = 0;          /* first Huffman code is zero */
  p = &zip->v[0];             /* grab values in bit order */
  h = -1;                     /* no tables yet--level -1 */
  w = l[-1] = 0;              /* no bits decoded yet */
  zip->u[0] = NULL;           /* just to keep compilers happy */
  q = NULL;                   /* ditto */
  z = 0;                      /* ditto */

  /* go through the bit lengths (k already is bits in shortest code) */
  for (; k <= g; k++) {
    a = c[k];
    while (a--) {
      /* here i is the Huffman code of length k bits for value *p */
      /* make tables up to required level */
      while (k > w + l[h]) {
        w += l[h++];            /* add bits already decoded */

        /* compute minimum size table less than or equal to *m bits */
        z = (z = g - w) > *m ? *m : z;        /* upper limit */
        if ((f = 1 << (j = k - w)) > a + 1)   /* try a k-w bit table */
        {                       /* too few codes for k-w bit table */
          f -= a + 1;           /* deduct codes from patterns left */
          xp = c + k;
          while (++j < z) {     /* try smaller tables up to z bits */
            if ((f <<= 1) <= *++xp) break; /* enough codes to use up j bits */
            f -= *xp;           /* else deduct codes from patterns */
          }
        }
        if (((w + j) > el) && (w < el)) {
	  j = el - w; /* make EOB code end at table */
	}
        z = 1 << j;             /* table entries for j-bit table */
        l[h] = j;               /* set table size in stack */

        /* allocate and link in new table */
	q = zip->sys->alloc(zip->sys, (z+1) * sizeof(struct mszipd_huft));
        if (!q) {
          if (h) mszipd_huft_free(zip, zip->u[0]);
          return 3;             /* not enough memory */
        }
        *t = q + 1;             /* link to list for mszipd_huft_free() */
        *(t = &(q->v.t)) = NULL;
        zip->u[h] = ++q;             /* table starts after link */

        /* connect to last table, if there is one */
        if (h) {
          zip->x[h] = i;             /* save pattern for backing up */
          r.b = l[h-1];    /* bits to dump before this table */
          r.e = (16 + j);  /* bits in this table */
          r.v.t = q;            /* pointer to this table */
          j = (i & ((1 << w) - 1)) >> (w - l[h-1]);
          zip->u[h-1][j] = r;        /* connect to last table */
        }
      }

      /* set up table entry in r */
      r.b = k - w;
      if (p >= &zip->v[n]) {
        r.e = 99;               /* out of values--invalid code */
      }
      else if (*p < s) {
        r.e = (*p < 256) ? 16 : 15;    /* 256 is end-of-block code */
        r.v.n = *p++;           /* simple code is just the value */
      }
      else {
        r.e = e[*p - s];   /* non-simple--look up in lists */
        r.v.n = d[*p++ - s];
      }

      /* fill code-like entries with r */
      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f) q[j] = r;

      /* backwards increment the k-bit code i */
      for (j = 1 << (k - 1); i & j; j >>= 1) i ^= j;
      i ^= j;

      /* backup over finished tables */
      /* don't need to update q */
      while ((i & ((1 << w) - 1)) != zip->x[h]) w -= l[--h];
    }
  }

  /* return actual size of base table */
  *m = l[0];

  /* Return true (1) if we were given an incomplete table */
  return (y != 0) && (g != 1);
}

static int mszipd_inflate_codes(struct mszipd_stream *zip,
				struct mszipd_huft *tl,
				struct mszipd_huft *td,
				int bl, int bd)
{
  register unsigned int e;  /* table entry flag/number of extra bits */
  unsigned int n, d;        /* length and index for copy */
  unsigned int w;           /* current window position */
  struct mszipd_huft *t; /* pointer to table entry */
  unsigned int ml, md;      /* masks for bl and bd bits */

  /* for the bit buffer */
  register unsigned int bit_buffer;
  register int bits_left;
  unsigned char *i_ptr, *i_end;


  /* make local copies of globals */
  RESTORE_BITS;
  w = zip->window_posn; /* initialize window position */

  /* inflate the coded data */

  /* precompute masks for speed */
  ml = mszipd_mask[bl];
  md = mszipd_mask[bd];

  for (;;) {
    NEED_BITS(bl);
    t = &tl[PEEK_BITS(ml)];
    e = t->e;
    if (e > 16) {
      do {
        if (e == 99) return 1;
        DUMP_BITS(t->b);
        e -= 16;
        NEED_BITS(e);
	t = &t->v.t[PEEK_BITS(mszipd_mask[e])];
	e = t->e;
      } while (e > 16);
    }
    DUMP_BITS(t->b);

    if (e == 16) {
      /* literal */
      zip->window[w++] = t->v.n;
    }
    else {
      /* EOB or length. exit if EOB */
      if (e == 15) break;

      /* get length of block to copy */
      READ_BITS(n, e);
      n += t->v.n;

      /* decode distance of block to copy */
      NEED_BITS(bd);
      t = &td[PEEK_BITS(md)];
      e = t->e;
      if (e > 16) {
        do {
          if (e == 99) return 1;
          DUMP_BITS(t->b);
	  e -= 16;
          NEED_BITS(e);
	  t = &t->v.t[PEEK_BITS(mszipd_mask[e])];
	  e = t->e;
        } while (e > 16);
      }
      DUMP_BITS(t->b);

      READ_BITS(d, e);
      d = w - t->v.n - d;

      /* copy matched block */
      do {
	d &= MSZIP_FRAME_SIZE-1;
	e = MSZIP_FRAME_SIZE - ((d > w) ? d : w);
	e = ((e > n) ? n : e);
        n -= e;
        do { zip->window[w++] = zip->window[d++]; } while (--e);
      } while (n);
    }
  }

  /* restore the globals from the locals */
  zip->window_posn = w;
  STORE_BITS;

  /* done */
  return 0;
}

int Zipinflate_dynamic(void)
{
  return 0;
}

int mszip_inflate(struct mszipd_stream *zip) {
  int last_block, block_type;

  /* for the bit buffer */
  register unsigned int bit_buffer;
  register int bits_left;
  unsigned char *i_ptr, *i_end;

  do {
    RESTORE_BITS;

    /* read in last block bit */
    READ_1BIT(last_block);

    /* read in block type */
    READ_2BITS(block_type);

    switch (block_type) {
    case 0:
      /* stored block */
      {
	unsigned int m, n;

	/* go to byte boundary */
	n = bits_left & 7;
	DUMP_BITS(n);

	/* get the length and its complement */
	READ_BITS(n, 16);
	READ_BITS(m, 16);
	if (n != ~m) return 1;

	/* read and output the compressed data */
	m = zip->window_posn;
	while (n--) {READ_8BITS(zip->window[m++]);}
      }
      break;
    case 1:
      /* fixed block */
      {
	struct mszipd_huft *fixed_tl;
	struct mszipd_huft *fixed_td;
	unsigned int *l = &zip->ll[0];
	int fixed_bl, fixed_bd;
	int i;

	/* literal table */
	i = 0;
	while (i < 144) l[i++] = 8;
	while (i < 256) l[i++] = 9;
	while (i < 280) l[i++] = 7;
	while (i < 288) l[i++] = 8;
	fixed_bl = 7;
	i = mszipd_huft_build(zip, l, 288, 257, mszipd_cplens, mszipd_cplext,
			      &fixed_tl, &fixed_bl);
	if (i) return i;

	/* distance table */
	/* make an incomplete code set */
	for (i = 0; i < 30; i++) l[i] = 5;
	fixed_bd = 5;
	i = mszipd_huft_build(zip, l, 30, 0, mszipd_cpdist, mszipd_cpdext,
			      &fixed_td, &fixed_bd);
	if (i > 1) {
	  mszipd_huft_free(zip, fixed_tl);
	  return i;
	}

	/* decompress until an end-of-block code */
	STORE_BITS;
	i = mszipd_inflate_codes(zip, fixed_tl, fixed_td, fixed_bl, fixed_bd);
	mszipd_huft_free(zip, fixed_td);
	mszipd_huft_free(zip, fixed_tl);
	if (i) return i;
      }
      break;
    case 2:
      /* dynamic block */
      {
	int i, bl, bd;
	unsigned int j, *ll, l, m, n, nb, nl, nd;
	struct mszipd_huft *tl, *td;

	ll = &zip->ll[0];

	/* read in table lengths */
	READ_5BITS(nl); nl += 257; /* number of bit length codes */
	READ_5BITS(nd); nd += 1;   /* number of distance codes */
	READ_4BITS(nb); nb += 4;   /* number of bit length codes */
	if ((nl > 288) || (nd > 32)) return 1; /* bad lengths */

	/* read in bit-length-code lengths */
	for (j = 0; j < nb; j++) READ_3BITS(ll[mszipd_border[j]]);
	while (j < 19) ll[mszipd_border[j++]] = 0;

	/* build decoding table for trees--single level, 7 bit lookup */
	bl = 7;
	i = mszipd_huft_build(zip, ll, 19, 19, NULL, NULL, &tl, &bl);
	if (i != 0) {
	  if(i == 1) mszipd_huft_free(zip, tl);
	  return i; /* incomplete code set */
	}

	/* read in literal and distance code lengths */
	n = nl + nd;
	m = mszipd_mask[bl];
	i = l = 0;
	while (i < n) {
	  NEED_BITS(bl);
	  td = &tl[PEEK_BITS(m)];
	  j = td->b;
	  DUMP_BITS(j);
	  j = td->v.n;
	  /* length of code in bits (0..15) */
	  if (j < 16) {
	    /* save last length in l */
	    ll[i++] = l = j;
	  }
	  else if (j == 16) {
	    /* repeat last length 3 to 6 times */
	    READ_2BITS(j); j += 3;
	    if((i + j) > n) return 1;
	    while (j--) ll[i++] = l;
	  }
	  else if (j == 17) {
	    /* 3 to 10 zero length codes */
	    READ_3BITS(j); j += 3;
	    if ((i + j) > n) return 1;
	    while (j--) ll[i++] = 0;
	    l = 0;
	  }
	  else {
	    /* j == 18: 11 to 138 zero length codes */
	    READ_7BITS(j); j += 11;
	    if ((i + j) > n) return 1;
	    while (j--) ll[i++] = 0;
	    l = 0;
	  }
	}
	/* free decoding table for trees */
	mszipd_huft_free(zip, tl);

	/* build the decoding tables for literal/length and distance codes */
	bl = MSZIPD_LBITS;
	i = mszipd_huft_build(zip, ll, nl, 257, mszipd_cplens, mszipd_cplext,
			      &tl, &bl);
	if (i != 0) {
	  if (i == 1) mszipd_huft_free(zip, tl);
	  return i; /* incomplete code set */
	}
	bd = MSZIPD_DBITS;
	mszipd_huft_build(zip, ll + nl, nd, 0, mszipd_cpdist, mszipd_cpdext,
			  &td, &bd);

	/* decompress until an end-of-block code */
	STORE_BITS;
	if (mszipd_inflate_codes(zip, tl, td, bl, bd)) return 1;

	/* free the decoding tables, return */
	mszipd_huft_free(zip, tl);
	mszipd_huft_free(zip, td);
      }
      break;
    default:
      /* bad block type */
      return 2;
    }
  } while (!last_block);

  /* return success */
  return 0;
}

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
  zip->bit_buffer = 0; zip->bits_left = 0;

  return zip;
}

int mszipd_decompress(struct mszipd_stream *zip, off_t out_bytes) {
  int i;

  /* for the bit buffer */
  register unsigned int bit_buffer;
  register int bits_left;
  unsigned char *i_ptr, *i_end;

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


  RESTORE_BITS;

  while (out_bytes > 0) {
    /* unpack another block */

    /* align to bytestream */
    i = bits_left & 7;
    DUMP_BITS(i);

    /* read 'CK' header */
    i = 0;
    while (i != 'K') {
      while (i != 'C') READ_8BITS(i);
      READ_8BITS(i);
    } while (i != 'K');
    zip->window_posn = 0;
    STORE_BITS;

  }

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
