/* cab extract 0.7 - a program to extract Microsoft Cabinet files
 * (C) 2000-2002 Stuart Caie <kyzer@4u.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* This is NOT a general purpose cabinet library with a front end tacked
 * on. If you want a comprehensive library to read and write cabinet
 * files, please get "libcabinet". If you want to create CAB files on UNIX
 * systems, get "Cablinux".
 *
 * Get the official Microsoft CAB SDK from here:
 *    http://www.powerload.fsnet.co.uk/download/cab-sdk.exe
 * You can use cabextract on this file to extract the contents.
 *
 * Many thanks to Dirk Stoecker, Matthew Russotto and Dave Tritscher and,
 * of course, Microsoft for the documentation they _did_ provide wholly
 * and accurately. MSZIP is a one-byte adaption of the deflate and inflate
 * methods created by Phil Katz. Quantum is based on the Quantum archiver,
 * created by David Stafford. LZX is an adaption of the LZX method created
 * by Jonathan Forbes and Tomi Poutanen.
 *
 * Furthermore, thanks to Jae Jung, for single-handedly fixing all the
 * bugs with LZX decompression in cabextract 0.1, and Eric Sharkey for the
 * original manual page.
 */

/* CAB files are 'cabinets'. 'Folders' store compressed data, and may span
 * several cabinets. 'Files' live as data inside a folder when
 * uncompressed. EOR checksums are used instead of CRCs. Four compression
 * formats are known - NONE, MSZIP, QUANTUM and LZX. NONE is obviously
 * uncompressed data. MSZIP is simply PKZIP's deflate/inflate algorithims
 * with 'CK' as a signature instead of 'PK'. QUANTUM is an LZ77 +
 * arithmetic coding method. LZX is a much loved LZH based archiver in the
 * Amiga world, the algorithm taken (bought?) by Microsoft and tweaked for
 * Intel code.
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <utime.h>
#include <dirent.h>
#include "getopt.h"
#define VERSION "x.x"

#include "md5.h"

#ifdef DEBUG
# define D(x) printf x ;
#else
# define D(x)
#endif

typedef unsigned char  UBYTE; /* 8 bits exactly    */
typedef unsigned short UWORD; /* 16 bits (or more) */
typedef unsigned int   ULONG; /* 32 bits (or more) */
typedef   signed int    LONG; /* 32 bits (or more) */

/* number of bits in a ULONG */
#ifndef CHAR_BIT
# define CHAR_BIT (8)
#endif
#define ULONG_BITS (sizeof(ULONG) * CHAR_BIT)

/* endian-neutral reading of little-endian data */
#define EndGetI32(a)  ((((a)[3])<<24)|(((a)[2])<<16)|(((a)[1])<<8)|((a)[0]))
#define EndGetI16(a)  ((((a)[1])<<8)|((a)[0]))

/* maximum number of cabinets any one folder can be split across */
#define CAB_SPLITMAX (10)

struct cabinet {
  struct cabinet *next;                /* for making a list of cabinets  */
  char  *filename;                     /* input name of cabinet          */
  FILE  *fh;                           /* open file handle or NULL       */
  off_t filelen;                       /* length of cabinet file         */
  off_t blocks_off;                    /* offset to data blocks in file  */
  struct cabinet *prevcab, *nextcab;   /* multipart cabinet chains       */
  char *prevname, *nextname;           /* and their filenames            */
  char *previnfo, *nextinfo;           /* and their visible names        */
  struct folder *folders;              /* first folder in this cabinet   */
  struct file *files;                  /* first file in this cabinet     */
  UBYTE block_resv;                    /* reserved space in datablocks   */
  UBYTE flags;                         /* header flags                   */
};

struct folder {
  struct folder *next;
  struct cabinet *cab[CAB_SPLITMAX];   /* cabinet(s) this folder spans   */
  off_t offset[CAB_SPLITMAX];          /* offset to data blocks          */
  UWORD comp_type;                     /* compression format/window size */
  ULONG comp_size;                     /* compressed size of folder      */
  UBYTE num_splits;                    /* number of split blocks + 1     */
  UWORD num_blocks;                    /* total number of blocks         */
  struct file *contfile;               /* the first split file           */
};

struct file {
  struct file *next;                   /* next file in sequence          */
  struct folder *folder;               /* folder that contains this file */
  char *filename;                      /* filename of file               */
  char *realname;                      /* the real file to output to     */
  FILE *fh;                            /* open file handle or NULL       */
  ULONG length;                        /* uncompressed length of file    */
  ULONG offset;                        /* uncompressed offset in folder  */
  UWORD index;                         /* magic index number of folder   */
  UWORD time, date, attribs;           /* MS-DOS time/date/attributes    */
};


/* structure offsets */
#define cfhead_Signature         (0x00)
#define cfhead_CabinetSize       (0x08)
#define cfhead_FileOffset        (0x10)
#define cfhead_MinorVersion      (0x18)
#define cfhead_MajorVersion      (0x19)
#define cfhead_NumFolders        (0x1A)
#define cfhead_NumFiles          (0x1C)
#define cfhead_Flags             (0x1E)
#define cfhead_SetID             (0x20)
#define cfhead_CabinetIndex      (0x22)
#define cfhead_SIZEOF            (0x24)
#define cfheadext_HeaderReserved (0x00)
#define cfheadext_FolderReserved (0x02)
#define cfheadext_DataReserved   (0x03)
#define cfheadext_SIZEOF         (0x04)
#define cffold_DataOffset        (0x00)
#define cffold_NumBlocks         (0x04)
#define cffold_CompType          (0x06)
#define cffold_SIZEOF            (0x08)
#define cffile_UncompressedSize  (0x00)
#define cffile_FolderOffset      (0x04)
#define cffile_FolderIndex       (0x08)
#define cffile_Date              (0x0A)
#define cffile_Time              (0x0C)
#define cffile_Attribs           (0x0E)
#define cffile_SIZEOF            (0x10)
#define cfdata_CheckSum          (0x00)
#define cfdata_CompressedSize    (0x04)
#define cfdata_UncompressedSize  (0x06)
#define cfdata_SIZEOF            (0x08)

/* flags */
#define cffoldCOMPTYPE_MASK            (0x000f)
#define cffoldCOMPTYPE_NONE            (0x0000)
#define cffoldCOMPTYPE_MSZIP           (0x0001)
#define cffoldCOMPTYPE_QUANTUM         (0x0002)
#define cffoldCOMPTYPE_LZX             (0x0003)
#define cfheadPREV_CABINET             (0x0001)
#define cfheadNEXT_CABINET             (0x0002)
#define cfheadRESERVE_PRESENT          (0x0004)
#define cffileCONTINUED_FROM_PREV      (0xFFFD)
#define cffileCONTINUED_TO_NEXT        (0xFFFE)
#define cffileCONTINUED_PREV_AND_NEXT  (0xFFFF)
#define cffile_A_RDONLY                (0x01)
#define cffile_A_HIDDEN                (0x02)
#define cffile_A_SYSTEM                (0x04)
#define cffile_A_ARCH                  (0x20)
#define cffile_A_EXEC                  (0x40)
#define cffile_A_NAME_IS_UTF           (0x80)


/*--------------------------------------------------------------------------*/
/* our archiver information / state */

/* MSZIP stuff */
#define ZIPWSIZE 	0x8000  /* window size */
#define ZIPLBITS	9	/* bits in base literal/length lookup table */
#define ZIPDBITS	6	/* bits in base distance lookup table */
#define ZIPBMAX		16      /* maximum bit length of any code */
#define ZIPN_MAX	288     /* maximum number of codes in any set */

struct Ziphuft {
  UBYTE e;                /* number of extra bits or operation */
  UBYTE b;                /* number of bits in this code or subcode */
  union {
    UWORD n;              /* literal, length base, or distance base */
    struct Ziphuft *t;    /* pointer to next level of table */
  } v;
};

struct ZIPstate {
    ULONG bb;              /* bit buffer */
    ULONG bk;              /* bits in bit buffer */
    ULONG ll[288+32];	   /* literal/length and distance code lengths */
    ULONG c[ZIPBMAX+1];    /* bit length count table */
    LONG  lx[ZIPBMAX+1];   /* memory for l[-1..ZIPBMAX-1] */
    struct Ziphuft *u[ZIPBMAX];         	/* table stack */
    ULONG v[ZIPN_MAX];     /* values in order of bit length */
    ULONG x[ZIPBMAX+1];    /* bit offsets, then code stack */
    UBYTE *inpos;
};
  
/* Quantum stuff */

struct QTMmodelsym {
  UWORD sym, cumfreq;
};

struct QTMmodel {
  int shiftsleft, entries; 
  struct QTMmodelsym *syms;
  UWORD tabloc[256];
};

struct QTMstate {
  /* selector model. 0-6 to say literal (0,1,2,3) or match (4,5,6) */
  struct QTMmodel model7;
  struct QTMmodelsym m7sym[7+1];

  /* match models 4,5,6pos encode positions. 4 and 5 are fixed length
   * matches, 6 is variable and encodes the length in model 6len.
   */
  struct QTMmodel model4, model5, model6pos, model6len;
  struct QTMmodelsym m4sym[0x18 + 1];
  struct QTMmodelsym m5sym[0x24 + 1];
  struct QTMmodelsym m6psym[0x2a + 1], m6lsym[0x1b + 1];

  /* literal models. Breaks 00-FF literals into 4 models: 00-3F (selector
   * 0), 40-7F (selector 1), 80-BF (selector 2) and C0-FF (selector 3).
   */
  struct QTMmodel model00, model40, model80, modelC0;
  struct QTMmodelsym m00sym[0x40 + 1], m40sym[0x40 + 1];
  struct QTMmodelsym m80sym[0x40 + 1], mC0sym[0x40 + 1];
};

/* LZX stuff */

/* some constants defined by the LZX specification */
#define LZX_MIN_MATCH                (2)
#define LZX_MAX_MATCH                (257)
#define LZX_NUM_CHARS                (256)
#define LZX_BLOCKTYPE_INVALID        (0)   /* also blocktypes 4-7 invalid */
#define LZX_BLOCKTYPE_VERBATIM       (1)
#define LZX_BLOCKTYPE_ALIGNED        (2)
#define LZX_BLOCKTYPE_UNCOMPRESSED   (3)
#define LZX_PRETREE_NUM_ELEMENTS     (20)
#define LZX_ALIGNED_NUM_ELEMENTS     (8)   /* aligned offset tree #elements */
#define LZX_NUM_PRIMARY_LENGTHS      (7)   /* this one missing from spec! */
#define LZX_NUM_SECONDARY_LENGTHS    (249) /* length tree #elements */

/* LZX huffman defines: tweak tablebits as desired */
#define LZX_PRETREE_MAXSYMBOLS  (LZX_PRETREE_NUM_ELEMENTS)
#define LZX_PRETREE_TABLEBITS   (6)
#define LZX_MAINTREE_MAXSYMBOLS (LZX_NUM_CHARS + 50*8)
#define LZX_MAINTREE_TABLEBITS  (12)
#define LZX_LENGTH_MAXSYMBOLS   (LZX_NUM_SECONDARY_LENGTHS+1)
#define LZX_LENGTH_TABLEBITS    (12)
#define LZX_ALIGNED_MAXSYMBOLS  (LZX_ALIGNED_NUM_ELEMENTS)
#define LZX_ALIGNED_TABLEBITS   (7)

#define LZX_LENTABLE_SAFETY (64) /* we allow length table decoding overruns */

#define LZX_DECLARE_TABLE(tbl) \
  UWORD tbl##_table[(1<<LZX_##tbl##_TABLEBITS) + (LZX_##tbl##_MAXSYMBOLS<<1)];\
  UBYTE tbl##_len  [LZX_##tbl##_MAXSYMBOLS + LZX_LENTABLE_SAFETY]

struct LZXstate {
  ULONG R0, R1, R2;      /* for the LRU offset system               */
  UWORD main_elements;   /* number of main tree elements            */
  int   header_read;     /* have we started decoding at all yet?    */
  UWORD block_type;      /* type of this block                      */
  ULONG block_length;    /* uncompressed length of this block       */
  ULONG block_remaining; /* uncompressed bytes still left to decode */
  ULONG frames_read;     /* the number of CFDATA blocks processed   */
  LONG  intel_filesize;  /* magic header value used for transform   */
  LONG  intel_curpos;    /* current offset in transform space       */
  int   intel_started;   /* have we seen any translatable data yet? */

  LZX_DECLARE_TABLE(PRETREE);
  LZX_DECLARE_TABLE(MAINTREE);
  LZX_DECLARE_TABLE(LENGTH);
  LZX_DECLARE_TABLE(ALIGNED);
};


/* generic stuff */
#define CAB(x) (decomp_state.x)
#define ZIP(x) (decomp_state.methods.zip.x)
#define QTM(x) (decomp_state.methods.qtm.x)
#define LZX(x) (decomp_state.methods.lzx.x)
#define DECR_OK           (0)
#define DECR_DATAFORMAT   (1)
#define DECR_ILLEGALDATA  (2)
#define DECR_NOMEMORY     (3)
#define DECR_CHECKSUM     (4)
#define DECR_INPUT        (5)
#define DECR_OUTPUT       (6)

/* CAB data blocks are <= 32768 bytes in uncompressed form. Uncompressed
 * blocks have zero growth. MSZIP guarantees that it won't grow above
 * uncompressed size by more than 12 bytes. LZX guarantees it won't grow
 * more than 6144 bytes.
 */
#define CAB_BLOCKMAX (32768)
#define CAB_INPUTMAX (CAB_BLOCKMAX+6144)

struct {
  struct folder *current; /* current folder we're extracting from  */
  ULONG offset;           /* uncompressed offset within folder     */
  UBYTE *outpos;          /* (high level) start of data to use up  */
  UWORD outlen;           /* (high level) amount of data to use up */
  UWORD split;            /* at which split in current folder?     */
  int (*decompress)(int, int); /* the chosen compression func      */
  UBYTE inbuf[CAB_INPUTMAX+2]; /* +2 for lzx bitbuffer overflows!  */
  UBYTE outbuf[CAB_BLOCKMAX];
  UBYTE *window;         /* LZX/QTM decoding window                */
  ULONG window_size;     /* LZX/QTM window size                    */
  ULONG actual_size;     /* window size when first allocated       */
  ULONG window_posn;     /* offset within window (also MSZIP)      */
  union {
    struct ZIPstate zip;
    struct QTMstate qtm;
    struct LZXstate lzx;
  } methods;
} decomp_state;


/* MSZIP decruncher */

/* Dirk Stoecker wrote the ZIP decoder, based on the InfoZip deflate code */

/* Tables for deflate from PKZIP's appnote.txt. */
static const UBYTE Zipborder[] = /* Order of the bit length code lengths */
{ 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static const UWORD Zipcplens[] = /* Copy lengths for literal codes 257..285 */
{ 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
static const UWORD Zipcplext[] = /* Extra bits for literal codes 257..285 */
{ 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
  4, 5, 5, 5, 5, 0, 99, 99}; /* 99==invalid */
static const UWORD Zipcpdist[] = /* Copy offsets for distance codes 0..29 */
{ 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const UWORD Zipcpdext[] = /* Extra bits for distance codes */
{ 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
10, 11, 11, 12, 12, 13, 13};

/* And'ing with Zipmask[n] masks the lower n bits */
static const UWORD Zipmask[17] = {
 0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
 0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

#define ZIPNEEDBITS(n) {while(k<(n)){LONG c=*(ZIP(inpos)++);\
    b|=((ULONG)c)<<k;k+=8;}}
#define ZIPDUMPBITS(n) {b>>=(n);k-=(n);}

void Ziphuft_free(struct Ziphuft *t)
{
  register struct Ziphuft *p, *q;

  /* Go through linked list, freeing from the allocated (t[-1]) address. */
  p = t;
  while (p != (struct Ziphuft *)NULL)
  {
    q = (--p)->v.t;
    free(p);
    p = q;
  } 
}

LONG Ziphuft_build(ULONG *b, ULONG n, ULONG s, UWORD *d, UWORD *e,
struct Ziphuft **t, LONG *m)
{
  ULONG a;                   	/* counter for codes of length k */
  ULONG el;                  	/* length of EOB code (value 256) */
  ULONG f;                   	/* i repeats in table every f entries */
  LONG g;                    	/* maximum code length */
  LONG h;                    	/* table level */
  register ULONG i;          	/* counter, current code */
  register ULONG j;          	/* counter */
  register LONG k;           	/* number of bits in current code */
  LONG *l;               	/* stack of bits per table */
  register ULONG *p;         	/* pointer into ZIP(c)[],ZIP(b)[],ZIP(v)[] */
  register struct Ziphuft *q;   /* points to current table */
  struct Ziphuft r;             /* table entry for structure assignment */
  register LONG w;              /* bits before this table == (l * h) */
  ULONG *xp;                 	/* pointer into x */
  LONG y;                       /* number of dummy codes added */
  ULONG z;                   	/* number of entries in current table */

  l = ZIP(lx)+1;

  /* Generate counts for each bit length */
  el = n > 256 ? b[256] : ZIPBMAX; /* set length of EOB code, if any */

  for(i = 0; i < ZIPBMAX+1; ++i)
    ZIP(c)[i] = 0;
  p = b;  i = n;
  do
  {
    ZIP(c)[*p]++; p++;               /* assume all entries <= ZIPBMAX */
  } while (--i);
  if (ZIP(c)[0] == n)                /* null input--all zero length codes */
  {
    *t = (struct Ziphuft *)NULL;
    *m = 0;
    return 0;
  }

  /* Find minimum and maximum length, bound *m by those */
  for (j = 1; j <= ZIPBMAX; j++)
    if (ZIP(c)[j])
      break;
  k = j;                        /* minimum code length */
  if ((ULONG)*m < j)
    *m = j;
  for (i = ZIPBMAX; i; i--)
    if (ZIP(c)[i])
      break;
  g = i;                        /* maximum code length */
  if ((ULONG)*m > i)
    *m = i;

  /* Adjust last length count to fill out codes, if needed */
  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= ZIP(c)[j]) < 0)
      return 2;                 /* bad input: more codes than bits */
  if ((y -= ZIP(c)[i]) < 0)
    return 2;
  ZIP(c)[i] += y;

  /* Generate starting offsets LONGo the value table for each length */
  ZIP(x)[1] = j = 0;
  p = ZIP(c) + 1;  xp = ZIP(x) + 2;
  while (--i)
  {                 /* note that i == g from above */
    *xp++ = (j += *p++);
  }

  /* Make a table of values in order of bit lengths */
  p = b;  i = 0;
  do{
    if ((j = *p++) != 0)
      ZIP(v)[ZIP(x)[j]++] = i;
  } while (++i < n);


  /* Generate the Huffman codes and for each, make the table entries */
  ZIP(x)[0] = i = 0;                 /* first Huffman code is zero */
  p = ZIP(v);                        /* grab values in bit order */
  h = -1;                       /* no tables yet--level -1 */
  w = l[-1] = 0;                /* no bits decoded yet */
  ZIP(u)[0] = (struct Ziphuft *)NULL;   /* just to keep compilers happy */
  q = (struct Ziphuft *)NULL;      /* ditto */
  z = 0;                        /* ditto */

  /* go through the bit lengths (k already is bits in shortest code) */
  for (; k <= g; k++)
  {
    a = ZIP(c)[k];
    while (a--)
    {
      /* here i is the Huffman code of length k bits for value *p */
      /* make tables up to required level */
      while (k > w + l[h])
      {
        w += l[h++];            /* add bits already decoded */

        /* compute minimum size table less than or equal to *m bits */
        z = (z = g - w) > (ULONG)*m ? *m : z;        /* upper limit */
        if ((f = 1 << (j = k - w)) > a + 1)     /* try a k-w bit table */
        {                       /* too few codes for k-w bit table */
          f -= a + 1;           /* deduct codes from patterns left */
          xp = ZIP(c) + k;
          while (++j < z)       /* try smaller tables up to z bits */
          {
            if ((f <<= 1) <= *++xp)
              break;            /* enough codes to use up j bits */
            f -= *xp;           /* else deduct codes from patterns */
          }
        }
        if ((ULONG)w + j > el && (ULONG)w < el)
          j = el - w;           /* make EOB code end at table */
        z = 1 << j;             /* table entries for j-bit table */
        l[h] = j;               /* set table size in stack */

        /* allocate and link in new table */
        if (!(q = (struct Ziphuft *) malloc((z + 1)*sizeof(struct Ziphuft))))
        {
          if(h)
            Ziphuft_free(ZIP(u)[0]);
          return 3;             /* not enough memory */
        }
        *t = q + 1;             /* link to list for Ziphuft_free() */
        *(t = &(q->v.t)) = (struct Ziphuft *)NULL;
        ZIP(u)[h] = ++q;             /* table starts after link */

        /* connect to last table, if there is one */
        if (h)
        {
          ZIP(x)[h] = i;             /* save pattern for backing up */
          r.b = (UBYTE)l[h-1];    /* bits to dump before this table */
          r.e = (UBYTE)(16 + j);  /* bits in this table */
          r.v.t = q;            /* pointer to this table */
          j = (i & ((1 << w) - 1)) >> (w - l[h-1]);
          ZIP(u)[h-1][j] = r;        /* connect to last table */
        }
      }

      /* set up table entry in r */
      r.b = (UBYTE)(k - w);
      if (p >= ZIP(v) + n)
        r.e = 99;               /* out of values--invalid code */
      else if (*p < s)
      {
        r.e = (UBYTE)(*p < 256 ? 16 : 15);    /* 256 is end-of-block code */
        r.v.n = *p++;           /* simple code is just the value */
      }
      else
      {
        r.e = (UBYTE)e[*p - s];   /* non-simple--look up in lists */
        r.v.n = d[*p++ - s];
      }

      /* fill code-like entries with r */
      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f)
        q[j] = r;

      /* backwards increment the k-bit code i */
      for (j = 1 << (k - 1); i & j; j >>= 1)
        i ^= j;
      i ^= j;

      /* backup over finished tables */
      while ((i & ((1 << w) - 1)) != ZIP(x)[h])
        w -= l[--h];            /* don't need to update q */
    }
  }

  /* return actual size of base table */
  *m = l[0];

  /* Return true (1) if we were given an incomplete table */
  return y != 0 && g != 1;
}

LONG Zipinflate_codes(struct Ziphuft *tl, struct Ziphuft *td,
LONG bl, LONG bd)
{
  register ULONG e;  /* table entry flag/number of extra bits */
  ULONG n, d;        /* length and index for copy */
  ULONG w;           /* current window position */
  struct Ziphuft *t; /* pointer to table entry */
  ULONG ml, md;      /* masks for bl and bd bits */
  register ULONG b;  /* bit buffer */
  register ULONG k;  /* number of bits in bit buffer */

  /* make local copies of globals */
  b = ZIP(bb);                       /* initialize bit buffer */
  k = ZIP(bk);
  w = CAB(window_posn);                       /* initialize window position */

  /* inflate the coded data */
  ml = Zipmask[bl];           	/* precompute masks for speed */
  md = Zipmask[bd];

  for(;;)
  {
    ZIPNEEDBITS((ULONG)bl)
    if((e = (t = tl + ((ULONG)b & ml))->e) > 16)
      do
      {
        if (e == 99)
          return 1;
        ZIPDUMPBITS(t->b)
        e -= 16;
        ZIPNEEDBITS(e)
      } while ((e = (t = t->v.t + ((ULONG)b & Zipmask[e]))->e) > 16);
    ZIPDUMPBITS(t->b)
    if (e == 16)                /* then it's a literal */
      CAB(outbuf)[w++] = (UBYTE)t->v.n;
    else                        /* it's an EOB or a length */
    {
      /* exit if end of block */
      if(e == 15)
        break;

      /* get length of block to copy */
      ZIPNEEDBITS(e)
      n = t->v.n + ((ULONG)b & Zipmask[e]);
      ZIPDUMPBITS(e);

      /* decode distance of block to copy */
      ZIPNEEDBITS((ULONG)bd)
      if ((e = (t = td + ((ULONG)b & md))->e) > 16)
        do {
          if (e == 99)
            return 1;
          ZIPDUMPBITS(t->b)
          e -= 16;
          ZIPNEEDBITS(e)
        } while ((e = (t = t->v.t + ((ULONG)b & Zipmask[e]))->e) > 16);
      ZIPDUMPBITS(t->b)
      ZIPNEEDBITS(e)
      d = w - t->v.n - ((ULONG)b & Zipmask[e]);
      ZIPDUMPBITS(e)
      do
      {
        n -= (e = (e = ZIPWSIZE - ((d &= ZIPWSIZE-1) > w ? d : w)) > n ?n:e);
        do
        {
          CAB(outbuf)[w++] = CAB(outbuf)[d++];
        } while (--e);
      } while (n);
    }
  }

  /* restore the globals from the locals */
  CAB(window_posn) = w;              /* restore global window pointer */
  ZIP(bb) = b;                       /* restore global bit buffer */
  ZIP(bk) = k;

  /* done */
  return 0;
}

LONG Zipinflate_stored(void)
/* "decompress" an inflated type 0 (stored) block. */
{
  ULONG n;           /* number of bytes in block */
  ULONG w;           /* current window position */
  register ULONG b;  /* bit buffer */
  register ULONG k;  /* number of bits in bit buffer */

  /* make local copies of globals */
  b = ZIP(bb);                       /* initialize bit buffer */
  k = ZIP(bk);
  w = CAB(window_posn);              /* initialize window position */

  /* go to byte boundary */
  n = k & 7;
  ZIPDUMPBITS(n);

  /* get the length and its complement */
  ZIPNEEDBITS(16)
  n = ((ULONG)b & 0xffff);
  ZIPDUMPBITS(16)
  ZIPNEEDBITS(16)
  if (n != (ULONG)((~b) & 0xffff))
    return 1;                   /* error in compressed data */
  ZIPDUMPBITS(16)

  /* read and output the compressed data */
  while(n--)
  {
    ZIPNEEDBITS(8)
    CAB(outbuf)[w++] = (UBYTE)b;
    ZIPDUMPBITS(8)
  }

  /* restore the globals from the locals */
  CAB(window_posn) = w;              /* restore global window pointer */
  ZIP(bb) = b;                       /* restore global bit buffer */
  ZIP(bk) = k;
  return 0;
}

LONG Zipinflate_fixed(void)
{
  struct Ziphuft *fixed_tl;
  struct Ziphuft *fixed_td;
  LONG fixed_bl, fixed_bd;
  LONG i;                /* temporary variable */
  ULONG *l;

  l = ZIP(ll);

  /* literal table */
  for(i = 0; i < 144; i++)
    l[i] = 8;
  for(; i < 256; i++)
    l[i] = 9;
  for(; i < 280; i++)
    l[i] = 7;
  for(; i < 288; i++)          /* make a complete, but wrong code set */
    l[i] = 8;
  fixed_bl = 7;
  if((i = Ziphuft_build(l, 288, 257, (UWORD *) Zipcplens,
  (UWORD *) Zipcplext, &fixed_tl, &fixed_bl)))
    return i;

  /* distance table */
  for(i = 0; i < 30; i++)      /* make an incomplete code set */
    l[i] = 5;
  fixed_bd = 5;
  if((i = Ziphuft_build(l, 30, 0, (UWORD *) Zipcpdist, (UWORD *) Zipcpdext,
  &fixed_td, &fixed_bd)) > 1)
  {
    Ziphuft_free(fixed_tl);
    return i;
  }

  /* decompress until an end-of-block code */
  i = Zipinflate_codes(fixed_tl, fixed_td, fixed_bl, fixed_bd);

  Ziphuft_free(fixed_td);
  Ziphuft_free(fixed_tl);
  return i;
}

LONG Zipinflate_dynamic(void)
 /* decompress an inflated type 2 (dynamic Huffman codes) block. */
{
  LONG i;          	/* temporary variables */
  ULONG j;
  ULONG *ll;
  ULONG l;           	/* last length */
  ULONG m;           	/* mask for bit lengths table */
  ULONG n;           	/* number of lengths to get */
  struct Ziphuft *tl;      /* literal/length code table */
  struct Ziphuft *td;      /* distance code table */
  LONG bl;              /* lookup bits for tl */
  LONG bd;              /* lookup bits for td */
  ULONG nb;          	/* number of bit length codes */
  ULONG nl;          	/* number of literal/length codes */
  ULONG nd;          	/* number of distance codes */
  register ULONG b;     /* bit buffer */
  register ULONG k;	/* number of bits in bit buffer */

  /* make local bit buffer */
  b = ZIP(bb);
  k = ZIP(bk);
  ll = ZIP(ll);

  /* read in table lengths */
  ZIPNEEDBITS(5)
  nl = 257 + ((ULONG)b & 0x1f);      /* number of literal/length codes */
  ZIPDUMPBITS(5)
  ZIPNEEDBITS(5)
  nd = 1 + ((ULONG)b & 0x1f);        /* number of distance codes */
  ZIPDUMPBITS(5)
  ZIPNEEDBITS(4)
  nb = 4 + ((ULONG)b & 0xf);         /* number of bit length codes */
  ZIPDUMPBITS(4)
  if(nl > 288 || nd > 32)
    return 1;                   /* bad lengths */

  /* read in bit-length-code lengths */
  for(j = 0; j < nb; j++)
  {
    ZIPNEEDBITS(3)
    ll[Zipborder[j]] = (ULONG)b & 7;
    ZIPDUMPBITS(3)
  }
  for(; j < 19; j++)
    ll[Zipborder[j]] = 0;

  /* build decoding table for trees--single level, 7 bit lookup */
  bl = 7;
  if((i = Ziphuft_build(ll, 19, 19, NULL, NULL, &tl, &bl)) != 0)
  {
    if(i == 1)
      Ziphuft_free(tl);
    return i;                   /* incomplete code set */
  }

  /* read in literal and distance code lengths */
  n = nl + nd;
  m = Zipmask[bl];
  i = l = 0;
  while((ULONG)i < n)
  {
    ZIPNEEDBITS((ULONG)bl)
    j = (td = tl + ((ULONG)b & m))->b;
    ZIPDUMPBITS(j)
    j = td->v.n;
    if (j < 16)                 /* length of code in bits (0..15) */
      ll[i++] = l = j;          /* save last length in l */
    else if (j == 16)           /* repeat last length 3 to 6 times */
    {
      ZIPNEEDBITS(2)
      j = 3 + ((ULONG)b & 3);
      ZIPDUMPBITS(2)
      if((ULONG)i + j > n)
        return 1;
      while (j--)
        ll[i++] = l;
    }
    else if (j == 17)           /* 3 to 10 zero length codes */
    {
      ZIPNEEDBITS(3)
      j = 3 + ((ULONG)b & 7);
      ZIPDUMPBITS(3)
      if ((ULONG)i + j > n)
        return 1;
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
    else                        /* j == 18: 11 to 138 zero length codes */
    {
      ZIPNEEDBITS(7)
      j = 11 + ((ULONG)b & 0x7f);
      ZIPDUMPBITS(7)
      if ((ULONG)i + j > n)
        return 1;
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
  }

  /* free decoding table for trees */
  Ziphuft_free(tl);

  /* restore the global bit buffer */
  ZIP(bb) = b;
  ZIP(bk) = k;

  /* build the decoding tables for literal/length and distance codes */
  bl = ZIPLBITS;
  if((i = Ziphuft_build(ll, nl, 257, (UWORD *) Zipcplens, (UWORD *) Zipcplext, &tl, &bl)) != 0)
  {
    if(i == 1)
      Ziphuft_free(tl);
    return i;                   /* incomplete code set */
  }
  bd = ZIPDBITS;
  Ziphuft_build(ll + nl, nd, 0, (UWORD *) Zipcpdist, (UWORD *) Zipcpdext, &td, &bd);

  /* decompress until an end-of-block code */
  if(Zipinflate_codes(tl, td, bl, bd))
    return 1;

  /* free the decoding tables, return */
  Ziphuft_free(tl);
  Ziphuft_free(td);
  return 0;
}

LONG Zipinflate_block(LONG *e) /* e == last block flag */
{ /* decompress an inflated block */
  ULONG t;           	/* block type */
  register ULONG b;     /* bit buffer */
  register ULONG k;     /* number of bits in bit buffer */

  /* make local bit buffer */
  b = ZIP(bb);
  k = ZIP(bk);

  /* read in last block bit */
  ZIPNEEDBITS(1)
  *e = (LONG)b & 1;
  ZIPDUMPBITS(1)

  /* read in block type */
  ZIPNEEDBITS(2)
  t = (ULONG)b & 3;
  ZIPDUMPBITS(2)

  /* restore the global bit buffer */
  ZIP(bb) = b;
  ZIP(bk) = k;

  /* inflate that block type */
  if(t == 2)
    return Zipinflate_dynamic();
  if(t == 0)
    return Zipinflate_stored();
  if(t == 1)
    return Zipinflate_fixed();
  /* bad block type */
  return 2;
}

int ZIPdecompress(int inlen, int outlen)
{
  LONG e;               /* last block flag */

  ZIP(inpos) = CAB(inbuf);
  ZIP(bb) = ZIP(bk) = CAB(window_posn) = 0;
  if(outlen > ZIPWSIZE)
    return DECR_DATAFORMAT;

  /* CK = Chris Kirmse, official Microsoft purloiner */
  if(ZIP(inpos)[0] != 0x43 || ZIP(inpos)[1] != 0x4B)
    return DECR_ILLEGALDATA;
  ZIP(inpos) += 2;

  do
  {
    if(Zipinflate_block(&e))
      return DECR_ILLEGALDATA;
  } while(!e);

  /* return success */
  return DECR_OK;
}

/* Quantum decruncher */

/* This decruncher was researched and implemented by Matthew Russotto. */
/* More info at http://www.speakeasy.org/~russotto/quantumcomp.html */
/* It has since been tidied up by Stuart Caie */

static UBYTE q_length_base[27], q_length_extra[27], q_extra_bits[42];
static ULONG q_position_base[42];

/* Initialise a model which decodes symbols from [s] to [s]+[n]-1 */
void QTMinitmodel(struct QTMmodel *m, struct QTMmodelsym *sym, int n, int s) {
  int i;
  m->shiftsleft = 4;
  m->entries    = n;
  m->syms       = sym;
  memset(m->tabloc, 0xFF, sizeof(m->tabloc)); /* clear out look-up table */
  for (i = 0; i < n; i++) {
    m->tabloc[i+s]     = i;   /* set up a look-up entry for symbol */
    m->syms[i].sym     = i+s; /* actual symbol */
    m->syms[i].cumfreq = n-i; /* current frequency of that symbol */
  }
  m->syms[n].cumfreq = 0;
}

int QTMinit(int window, int level) {
  int wndsize = 1 << window, msz = window * 2, i;
  ULONG j;

  /* QTM supports window sizes of 2^10 (1Kb) through 2^21 (2Mb) */
  /* if a previously allocated window is big enough, keep it    */
  if (window < 10 || window > 21) return DECR_DATAFORMAT;

  if (CAB(actual_size) < wndsize) {
    if (CAB(window)) free((void *) CAB(window));
    CAB(window) = NULL;
    CAB(actual_size) = 0;
  }
  if (CAB(window) == NULL) {
    if (!(CAB(window) = (UBYTE *) malloc(wndsize))) return DECR_NOMEMORY;
    CAB(actual_size) = wndsize;
  }
  CAB(window_size) = wndsize;
  CAB(window_posn) = 0;

  /* initialise static slot/extrabits tables */
  for (i = 0, j = 0; i < 27; i++) {
    q_length_extra[i] = (i == 26) ? 0 : (i < 2 ? 0 : i - 2) >> 2;
    q_length_base[i] = j; j += 1 << ((i == 26) ? 5 : q_length_extra[i]);
  }
  for (i = 0, j = 0; i < 42; i++) {
    q_extra_bits[i] = (i < 2 ? 0 : i-2) >> 1;
    q_position_base[i] = j; j += 1 << q_extra_bits[i];
  }

  /* initialise arithmetic coding models */

  QTMinitmodel(&QTM(model7), &QTM(m7sym)[0], 7, 0);

  QTMinitmodel(&QTM(model00), &QTM(m00sym)[0], 0x40, 0x00);
  QTMinitmodel(&QTM(model40), &QTM(m40sym)[0], 0x40, 0x40);
  QTMinitmodel(&QTM(model80), &QTM(m80sym)[0], 0x40, 0x80);
  QTMinitmodel(&QTM(modelC0), &QTM(mC0sym)[0], 0x40, 0xC0);

  /* model 4 depends on table size, ranges from 20 to 24  */
  QTMinitmodel(&QTM(model4), &QTM(m4sym)[0], (msz < 24) ? msz : 24, 0);
  /* model 5 depends on table size, ranges from 20 to 36  */
  QTMinitmodel(&QTM(model5), &QTM(m5sym)[0], (msz < 36) ? msz : 36, 0);
  /* model 6pos depends on table size, ranges from 20 to 42 */
  QTMinitmodel(&QTM(model6pos), &QTM(m6psym)[0], msz, 0);
  QTMinitmodel(&QTM(model6len), &QTM(m6lsym)[0], 27, 0);

  return DECR_OK;
}


void QTMupdatemodel(struct QTMmodel *model, int sym) {
  struct QTMmodelsym temp;
  int i, j;

  for (i = 0; i < sym; i++) model->syms[i].cumfreq += 8;

  if (model->syms[0].cumfreq > 3800) {
    if (--model->shiftsleft) {
      for (i = model->entries - 1; i >= 0; i--) {
	/* -1, not -2; the 0 entry saves this */
	model->syms[i].cumfreq >>= 1;
	if (model->syms[i].cumfreq <= model->syms[i+1].cumfreq) {
	  model->syms[i].cumfreq = model->syms[i+1].cumfreq + 1;
	}
      }
    }
    else {
      model->shiftsleft = 50;
      for (i = 0; i < model->entries ; i++) {
	/* no -1, want to include the 0 entry */
	/* this converts cumfreqs into frequencies, then shifts right */
	model->syms[i].cumfreq -= model->syms[i+1].cumfreq;
	model->syms[i].cumfreq++; /* avoid losing things entirely */
	model->syms[i].cumfreq >>= 1;
      }

      /* now sort by frequencies, decreasing order -- this must be an
       * inplace selection sort, or a sort with the same (in)stability
       * characteristics
       */
      for (i = 0; i < model->entries - 1; i++) {
	for (j = i + 1; j < model->entries; j++) {
	  if (model->syms[i].cumfreq < model->syms[j].cumfreq) {
	    temp = model->syms[i];
	    model->syms[i] = model->syms[j];
	    model->syms[j] = temp;
	  }
	}
      }
    
      /* then convert frequencies back to cumfreq */
      for (i = model->entries - 1; i >= 0; i--) {
	model->syms[i].cumfreq += model->syms[i+1].cumfreq;
      }
      /* then update the other part of the table */
      for (i = 0; i < model->entries; i++) {
	model->tabloc[model->syms[i].sym] = i;
      }
    }
  }
}

/* Bitstream reading macros (Quantum / normal byte order)
 *
 * Q_INIT_BITSTREAM    should be used first to set up the system
 * Q_READ_BITS(var,n)  takes N bits from the buffer and puts them in var.
 *                     unlike LZX, this can loop several times to get the
 *                     requisite number of bits.
 * Q_FILL_BUFFER       adds more data to the bit buffer, if there is room
 *                     for another 16 bits.
 * Q_PEEK_BITS(n)      extracts (without removing) N bits from the bit
 *                     buffer
 * Q_REMOVE_BITS(n)    removes N bits from the bit buffer
 *
 * These bit access routines work by using the area beyond the MSB and the
 * LSB as a free source of zeroes. This avoids having to mask any bits.
 * So we have to know the bit width of the bitbuffer variable. This is
 * defined as ULONG_BITS.
 *
 * ULONG_BITS should be at least 16 bits. Unlike LZX's Huffman decoding,
 * Quantum's arithmetic decoding only needs 1 bit at a time, it doesn't
 * need an assured number. Retrieving larger bitstrings can be done with
 * multiple reads and fills of the bitbuffer. The code should work fine
 * for machines where ULONG >= 32 bits.
 *
 * Also note that Quantum reads bytes in normal order; LZX is in
 * little-endian order.
 */

#define Q_INIT_BITSTREAM do { bitsleft = 0; bitbuf = 0; } while (0)

#define Q_FILL_BUFFER do {                                              \
  if (bitsleft <= (ULONG_BITS - 16)) {                                  \
    bitbuf |= ((inpos[0]<<8)|inpos[1]) << (ULONG_BITS-16 - bitsleft);   \
    bitsleft += 16; inpos += 2;                                         \
  }                                                                     \
} while (0)

#define Q_PEEK_BITS(n)   (bitbuf >> (ULONG_BITS - (n)))
#define Q_REMOVE_BITS(n) ((bitbuf <<= (n)), (bitsleft -= (n)))

#define Q_READ_BITS(v,n) do {						\
  (v) = 0;                                                              \
  for (bitsneed = (n); bitsneed; bitsneed -= bitrun) {                  \
    Q_FILL_BUFFER;                                                      \
    bitrun = (bitsneed > bitsleft) ? bitsleft : bitsneed;               \
    (v) = ((v) << bitrun) | Q_PEEK_BITS(bitrun);                        \
    Q_REMOVE_BITS(bitrun);                                              \
  }                                                                     \
} while (0)

#define Q_MENTRIES(model) (QTM(model).entries)
#define Q_MSYM(model,symidx) (QTM(model).syms[(symidx)].sym)
#define Q_MSYMFREQ(model,symidx) (QTM(model).syms[(symidx)].cumfreq)

/* GET_SYMBOL(model, var) fetches the next symbol from the stated model
 * and puts it in var. it may need to read the bitstream to do this.
 */
#define GET_SYMBOL(m, var) do {                                         \
  range =  ((H - L) & 0xFFFF) + 1;                                      \
  symf = ((((C - L + 1) * Q_MSYMFREQ(m,0)) - 1) / range) & 0xFFFF;      \
                                                                        \
  for (i=1; i < Q_MENTRIES(m); i++) {                                   \
    if (Q_MSYMFREQ(m,i) <= symf) break;                                 \
  }                                                                     \
  (var) = Q_MSYM(m,i-1);                                                \
                                                                        \
  range = (H - L) + 1;                                                  \
  H = L + ((Q_MSYMFREQ(m,i-1) * range) / Q_MSYMFREQ(m,0)) - 1;          \
  L = L + ((Q_MSYMFREQ(m,i)   * range) / Q_MSYMFREQ(m,0));              \
  while (1) {                                                           \
    if ((L & 0x8000) != (H & 0x8000)) {                                 \
      if ((L & 0x4000) && !(H & 0x4000)) {                              \
        /* underflow case */                                            \
        C ^= 0x4000; L &= 0x3FFF; H |= 0x4000;                          \
      }                                                                 \
      else break;                                                       \
    }                                                                   \
    L <<= 1; H = (H << 1) | 1;                                          \
    Q_FILL_BUFFER;                                                      \
    C  = (C << 1) | Q_PEEK_BITS(1);                                     \
    Q_REMOVE_BITS(1);                                                   \
  }                                                                     \
                                                                        \
  QTMupdatemodel(&(QTM(m)), i);                                         \
} while (0)


int QTMdecompress(int inlen, int outlen) {
  UBYTE *inpos  = CAB(inbuf);
  UBYTE *window = CAB(window);
  UBYTE *runsrc, *rundest;

  ULONG window_posn = CAB(window_posn);
  ULONG window_size = CAB(window_size);

  /* used by bitstream macros */
  register int bitsleft, bitrun, bitsneed;
  register ULONG bitbuf;

  /* used by GET_SYMBOL */
  ULONG range;
  UWORD symf;
  int i;

  int extra, togo = outlen, match_length, copy_length;
  UBYTE selector, sym;
  ULONG match_offset;

  UWORD H = 0xFFFF, L = 0, C;

  /* read initial value of C */
  Q_INIT_BITSTREAM;
  Q_READ_BITS(C, 16);

  /* apply 2^x-1 mask */
  window_posn &= window_size - 1;
  /* runs can't straddle the window wraparound */
  if ((window_posn + togo) > window_size) {
    D(("straddled run\n"))
    return DECR_DATAFORMAT;
  }

  while (togo > 0) {
    GET_SYMBOL(model7, selector);
    switch (selector) {
    case 0:
      GET_SYMBOL(model00, sym); window[window_posn++] = sym; togo--;  break;
    case 1:
      GET_SYMBOL(model40, sym); window[window_posn++] = sym; togo--;  break;
    case 2:
      GET_SYMBOL(model80, sym); window[window_posn++] = sym; togo--;  break;
    case 3:
      GET_SYMBOL(modelC0, sym); window[window_posn++] = sym; togo--;  break;

    case 4:
      /* selector 4 = fixed length of 3 */
      GET_SYMBOL(model4, sym);
      Q_READ_BITS(extra, q_extra_bits[sym]);
      match_offset = q_position_base[sym] + extra + 1;
      match_length = 3;
      break;

    case 5:
      /* selector 5 = fixed length of 4 */
      GET_SYMBOL(model5, sym);
      Q_READ_BITS(extra, q_extra_bits[sym]);
      match_offset = q_position_base[sym] + extra + 1;
      match_length = 4;
      break;

    case 6:
      /* selector 6 = variable length */
      GET_SYMBOL(model6len, sym);
      Q_READ_BITS(extra, q_length_extra[sym]);
      match_length = q_length_base[sym] + extra + 5;
      GET_SYMBOL(model6pos, sym);
      Q_READ_BITS(extra, q_extra_bits[sym]);
      match_offset = q_position_base[sym] + extra + 1;
      break;

    default:
      D(("Selector is bogus\n"))
      return DECR_ILLEGALDATA;
    }

    /* if this is a match */
    if (selector >= 4) {
      rundest = window + window_posn;
      togo -= match_length;

      /* copy any wrapped around source data */
      if (window_posn >= match_offset) {
        /* no wrap */
        runsrc = rundest - match_offset;
      } else {
        runsrc = rundest + (window_size - match_offset);
        copy_length = match_offset - window_posn;
        if (copy_length < match_length) {
          match_length -= copy_length;
          window_posn += copy_length;
          while (copy_length-- > 0) *rundest++ = *runsrc++;
          runsrc = window;
        }
      }
      window_posn += match_length;

      /* copy match data - no worries about destination wraps */
      while (match_length-- > 0) *rundest++ = *runsrc++;
    }
  } /* while (togo > 0) */

  if (togo != 0) {
    D(("Frame overflow, this_run = %d\n", togo))
    return DECR_ILLEGALDATA;
  }

  memcpy(CAB(outbuf), window + ((!window_posn) ? window_size : window_posn) -
    outlen, outlen);

  CAB(window_posn) = window_posn;
  return DECR_OK;
}



/* LZX decruncher */

/* Microsoft's LZX document and their implementation of the
 * com.ms.util.cab Java package do not concur.
 *
 * In the LZX document, there is a table showing the correlation between
 * window size and the number of position slots. It states that the 1MB
 * window = 40 slots and the 2MB window = 42 slots. In the implementation,
 * 1MB = 42 slots, 2MB = 50 slots. The actual calculation is 'find the
 * first slot whose position base is equal to or more than the required
 * window size'. This would explain why other tables in the document refer
 * to 50 slots rather than 42.
 *
 * The constant NUM_PRIMARY_LENGTHS used in the decompression pseudocode
 * is not defined in the specification.
 *
 * The LZX document does not state the uncompressed block has an
 * uncompressed length field. Where does this length field come from, so
 * we can know how large the block is? The implementation has it as the 24
 * bits following after the 3 blocktype bits, before the alignment
 * padding.
 *
 * The LZX document states that aligned offset blocks have their aligned
 * offset huffman tree AFTER the main and length trees. The implementation
 * suggests that the aligned offset tree is BEFORE the main and length
 * trees.
 *
 * The LZX document decoding algorithm states that, in an aligned offset
 * block, if an extra_bits value is 1, 2 or 3, then that number of bits
 * should be read and the result added to the match offset. This is
 * correct for 1 and 2, but not 3, where just a huffman symbol (using the
 * aligned tree) should be read.
 *
 * Regarding the E8 preprocessing, the LZX document states 'No translation
 * may be performed on the last 6 bytes of the input block'. This is
 * correct.  However, the pseudocode provided checks for the *E8 leader*
 * up to the last 6 bytes. If the leader appears between -10 and -7 bytes
 * from the end, this would cause the next four bytes to be modified, at
 * least one of which would be in the last 6 bytes, which is not allowed
 * according to the spec.
 *
 * The specification states that the huffman trees must always contain at
 * least one element. However, many CAB files contain blocks where the
 * length tree is completely empty (because there are no matches), and
 * this is expected to succeed.
 */


/* LZX uses what it calls 'position slots' to represent match offsets.
 * What this means is that a small 'position slot' number and a small
 * offset from that slot are encoded instead of one large offset for
 * every match.
 * - lzx_position_base is an index to the position slot bases
 * - lzx_extra_bits states how many bits of offset-from-base data is needed.
 */
static ULONG lzx_position_base[51];
static UBYTE extra_bits[51];

int LZXinit(int window) {
  ULONG wndsize = 1 << window;
  int i, j, posn_slots;

  /* LZX supports window sizes of 2^15 (32Kb) through 2^21 (2Mb) */
  /* if a previously allocated window is big enough, keep it     */
  if (window < 15 || window > 21) return DECR_DATAFORMAT;

  if (CAB(actual_size) < wndsize) {
    if (CAB(window)) free((void *) CAB(window));
    CAB(window) = NULL;
    CAB(actual_size) = 0;
  }
  if (CAB(window) == NULL) {
    if (!(CAB(window) = (UBYTE *) malloc(wndsize))) return DECR_NOMEMORY;
    memset(CAB(window), 0xDC, wndsize);
    CAB(actual_size) = wndsize;
  }
  CAB(window_size) = wndsize;
  CAB(window_posn) = 0;

  /* initialise static tables */
  for (i=0, j=0; i <= 50; i += 2) {
    extra_bits[i] = extra_bits[i+1] = j; /* 0,0,0,0,1,1,2,2,3,3... */
    if ((i != 0) && (j < 17)) j++; /* 0,0,1,2,3,4...15,16,17,17,17,17... */
  }
  for (i=0, j=0; i <= 50; i++) {
    lzx_position_base[i] = j; /* 0,1,2,3,4,6,8,12,16,24,32,... */
    j += 1 << extra_bits[i]; /* 1,1,1,1,2,2,4,4,8,8,16,16,32,32,... */
  }

  /* calculate required position slots */
       if (window == 20) posn_slots = 42;
  else if (window == 21) posn_slots = 50;
  else posn_slots = window << 1;

  /*posn_slots=i=0; while (i < wndsize) i += 1 << extra_bits[posn_slots++]; */
  

  LZX(R0)  =  LZX(R1)  = LZX(R2) = 1;
  LZX(main_elements)   = LZX_NUM_CHARS + (posn_slots << 3);
  LZX(header_read)     = 0;
  LZX(frames_read)     = 0;
  LZX(block_remaining) = 0;
  LZX(block_type)      = LZX_BLOCKTYPE_INVALID;
  LZX(intel_curpos)    = 0;
  LZX(intel_started)   = 0;

  /* initialise tables to 0 (because deltas will be applied to them) */
  for (i = 0; i < LZX_MAINTREE_MAXSYMBOLS; i++) LZX(MAINTREE_len)[i] = 0;
  for (i = 0; i < LZX_LENGTH_MAXSYMBOLS; i++)   LZX(LENGTH_len)[i]   = 0;

  return DECR_OK;
}

/* Bitstream reading macros (LZX / intel little-endian byte order)
 *
 * INIT_BITSTREAM    should be used first to set up the system
 * READ_BITS(var,n)  takes N bits from the buffer and puts them in var
 *
 * ENSURE_BITS(n)    ensures there are at least N bits in the bit buffer.
 *                   it can guarantee up to 17 bits (i.e. it can read in
 *                   16 new bits when there is down to 1 bit in the buffer,
 *                   and it can read 32 bits when there are 0 bits in the
 *                   buffer).
 * PEEK_BITS(n)      extracts (without removing) N bits from the bit buffer
 * REMOVE_BITS(n)    removes N bits from the bit buffer
 *
 * These bit access routines work by using the area beyond the MSB and the
 * LSB as a free source of zeroes. This avoids having to mask any bits.
 * So we have to know the bit width of the bitbuffer variable.
 */

#define INIT_BITSTREAM do { bitsleft = 0; bitbuf = 0; } while (0)

/* Quantum reads bytes in normal order; LZX is little-endian order */
#define ENSURE_BITS(n)                                                  \
  while (bitsleft < (n)) {					        \
    bitbuf |= ((inpos[1]<<8)|inpos[0]) << (ULONG_BITS-16 - bitsleft);	\
    bitsleft += 16; inpos+=2;						\
  }

#define PEEK_BITS(n)   (bitbuf >> (ULONG_BITS - (n)))
#define REMOVE_BITS(n) ((bitbuf <<= (n)), (bitsleft -= (n)))

#define READ_BITS(v,n) do {						\
  if (n) {								\
    ENSURE_BITS(n);							\
    (v) = PEEK_BITS(n);							\
    REMOVE_BITS(n);							\
  }									\
  else {								\
    (v) = 0;								\
  }									\
} while (0)

/* Huffman macros */

#define TABLEBITS(tbl)   (LZX_##tbl##_TABLEBITS)
#define MAXSYMBOLS(tbl)  (LZX_##tbl##_MAXSYMBOLS)
#define SYMTABLE(tbl)    (LZX(tbl##_table))
#define LENTABLE(tbl)    (LZX(tbl##_len))

/* BUILD_TABLE(tablename) builds a huffman lookup table from code lengths.
 * In reality, it just calls make_decode_table() with the appropriate
 * values - they're all fixed by some #defines anyway, so there's no point
 * writing each call out in full by hand.
 */
#define BUILD_TABLE(tbl)						\
  if (make_decode_table(						\
    MAXSYMBOLS(tbl), TABLEBITS(tbl), LENTABLE(tbl), SYMTABLE(tbl)	\
  )) { return DECR_ILLEGALDATA; }


/* READ_HUFFSYM(tablename, var) decodes one huffman symbol from the
 * bitstream using the stated table and puts it in var.
 */
#define READ_HUFFSYM(tbl,var) do {					\
  ENSURE_BITS(16);							\
  hufftbl = SYMTABLE(tbl);						\
  if ((i = hufftbl[PEEK_BITS(TABLEBITS(tbl))]) >= MAXSYMBOLS(tbl)) {	\
    j = 1 << (ULONG_BITS - TABLEBITS(tbl));				\
    do {								\
      j >>= 1; i <<= 1; i |= (bitbuf & j) ? 1 : 0;			\
      if (!j) { return DECR_ILLEGALDATA; }	                        \
    } while ((i = hufftbl[i]) >= MAXSYMBOLS(tbl));			\
  }									\
  j = LENTABLE(tbl)[(var) = i];						\
  REMOVE_BITS(j);							\
} while (0)


/* READ_LENGTHS(tablename, first, last) reads in code lengths for symbols
 * first to last in the given table. The code lengths are stored in their
 * own special LZX way.
 */
#define READ_LENGTHS(tbl,first,last) do { \
  lb.bb = bitbuf; lb.bl = bitsleft; lb.ip = inpos; \
  if (lzx_read_lens(LENTABLE(tbl),(first),(last),&lb)) { \
    return DECR_ILLEGALDATA; \
  } \
  bitbuf = lb.bb; bitsleft = lb.bl; inpos = lb.ip; \
} while (0)


/* make_decode_table(nsyms, nbits, length[], table[])
 *
 * This function was coded by David Tritscher. It builds a fast huffman
 * decoding table out of just a canonical huffman code lengths table.
 *
 * nsyms  = total number of symbols in this huffman tree.
 * nbits  = any symbols with a code length of nbits or less can be decoded
 *          in one lookup of the table.
 * length = A table to get code lengths from [0 to syms-1]
 * table  = The table to fill up with decoded symbols and pointers.
 *
 * Returns 0 for OK or 1 for error
 */

int make_decode_table(ULONG nsyms, ULONG nbits, UBYTE *length, UWORD *table) {
  register UWORD sym;
  register ULONG leaf;
  register UBYTE bit_num = 1;
  ULONG fill;
  ULONG pos         = 0; /* the current position in the decode table */
  ULONG table_mask  = 1 << nbits;
  ULONG bit_mask    = table_mask >> 1; /* don't do 0 length codes */
  ULONG next_symbol = bit_mask; /* base of allocation for long codes */

  /* fill entries for codes short enough for a direct mapping */
  while (bit_num <= nbits) {
    for (sym = 0; sym < nsyms; sym++) {
      if (length[sym] == bit_num) {
        leaf = pos;

        if((pos += bit_mask) > table_mask) return 1; /* table overrun */

        /* fill all possible lookups of this symbol with the symbol itself */
        fill = bit_mask;
        while (fill-- > 0) table[leaf++] = sym;
      }
    }
    bit_mask >>= 1;
    bit_num++;
  }

  /* if there are any codes longer than nbits */
  if (pos != table_mask) {
    /* clear the remainder of the table */
    for (sym = pos; sym < table_mask; sym++) table[sym] = 0;

    /* give ourselves room for codes to grow by up to 16 more bits */
    pos <<= 16;
    table_mask <<= 16;
    bit_mask = 1 << 15;

    while (bit_num <= 16) {
      for (sym = 0; sym < nsyms; sym++) {
        if (length[sym] == bit_num) {
          leaf = pos >> 16;
          for (fill = 0; fill < bit_num - nbits; fill++) {
            /* if this path hasn't been taken yet, 'allocate' two entries */
            if (table[leaf] == 0) {
              table[(next_symbol << 1)] = 0;
              table[(next_symbol << 1) + 1] = 0;
              table[leaf] = next_symbol++;
            }
            /* follow the path and select either left or right for next bit */
            leaf = table[leaf] << 1;
            if ((pos >> (15-fill)) & 1) leaf++;
          }
          table[leaf] = sym;

          if ((pos += bit_mask) > table_mask) return 1; /* table overflow */
        }
      }
      bit_mask >>= 1;
      bit_num++;
    }
  }

  /* full table? */
  if (pos == table_mask) return 0;

  /* either erroneous table, or all elements are 0 - let's find out. */
  for (sym = 0; sym < nsyms; sym++) if (length[sym]) return 1;
  return 0;
}

struct lzx_bits {
  ULONG bb;
  int bl;
  UBYTE *ip;
};

int lzx_read_lens(UBYTE *lens, ULONG first, ULONG last, struct lzx_bits *lb) {
  ULONG i,j, x,y;
  int z;

  register ULONG bitbuf = lb->bb;
  register int bitsleft = lb->bl;
  UBYTE *inpos = lb->ip;
  UWORD *hufftbl;
  
  for (x = 0; x < 20; x++) {
    READ_BITS(y, 4);
    LENTABLE(PRETREE)[x] = y;
  }
  BUILD_TABLE(PRETREE);

  for (x = first; x < last; ) {
    READ_HUFFSYM(PRETREE, z);
    if (z == 17) {
      READ_BITS(y, 4); y += 4;
      while (y--) lens[x++] = 0;
    }
    else if (z == 18) {
      READ_BITS(y, 5); y += 20;
      while (y--) lens[x++] = 0;
    }
    else if (z == 19) {
      READ_BITS(y, 1); y += 4;
      READ_HUFFSYM(PRETREE, z);
      z = lens[x] - z; if (z < 0) z += 17;
      while (y--) lens[x++] = z;
    }
    else {
      z = lens[x] - z; if (z < 0) z += 17;
      lens[x++] = z;
    }
  }

  lb->bb = bitbuf;
  lb->bl = bitsleft;
  lb->ip = inpos;
  return 0;
}

int LZXdecompress(int inlen, int outlen) {
  UBYTE *inpos  = CAB(inbuf);
  UBYTE *endinp = inpos + inlen;
  UBYTE *window = CAB(window);
  UBYTE *runsrc, *rundest;
  UWORD *hufftbl; /* used in READ_HUFFSYM macro as chosen decoding table */

  ULONG window_posn = CAB(window_posn);
  ULONG window_size = CAB(window_size);
  ULONG R0 = LZX(R0);
  ULONG R1 = LZX(R1);
  ULONG R2 = LZX(R2);

  register ULONG bitbuf;
  register int bitsleft;
  ULONG match_offset, i,j,k; /* ijk used in READ_HUFFSYM macro */
  struct lzx_bits lb; /* used in READ_LENGTHS macro */

  int togo = outlen, this_run, main_element, aligned_bits;
  int match_length, copy_length, length_footer, extra, verbatim_bits;

  INIT_BITSTREAM;

  /* read header if necessary */
  if (!LZX(header_read)) {
    i = j = 0;
    READ_BITS(k, 1); if (k) { READ_BITS(i,16); READ_BITS(j,16); }
    LZX(intel_filesize) = (i << 16) | j; /* or 0 if not encoded */
    LZX(header_read) = 1;
  }

  /* main decoding loop */
  while (togo > 0) {
    /* last block finished, new block expected */
    if (LZX(block_remaining) == 0) {
      if (LZX(block_type) == LZX_BLOCKTYPE_UNCOMPRESSED) {
        if (LZX(block_length) & 1) inpos++; /* realign bitstream to word */
        INIT_BITSTREAM;
      }

      READ_BITS(LZX(block_type), 3);
      READ_BITS(i, 16);
      READ_BITS(j, 8);
      LZX(block_remaining) = LZX(block_length) = (i << 8) | j;

      switch (LZX(block_type)) {
      case LZX_BLOCKTYPE_ALIGNED:
        for (i = 0; i < 8; i++) { READ_BITS(j, 3); LENTABLE(ALIGNED)[i] = j; }
        BUILD_TABLE(ALIGNED);
        /* rest of aligned header is same as verbatim */

      case LZX_BLOCKTYPE_VERBATIM:
        READ_LENGTHS(MAINTREE, 0, 256);
        READ_LENGTHS(MAINTREE, 256, LZX(main_elements));
        BUILD_TABLE(MAINTREE);
        if (LENTABLE(MAINTREE)[0xE8] != 0) LZX(intel_started) = 1;

        READ_LENGTHS(LENGTH, 0, LZX_NUM_SECONDARY_LENGTHS);
        BUILD_TABLE(LENGTH);
        break;

      case LZX_BLOCKTYPE_UNCOMPRESSED:
        LZX(intel_started) = 1; /* because we can't assume otherwise */
        ENSURE_BITS(16); /* get up to 16 pad bits into the buffer */
        if (bitsleft > 16) inpos -= 2; /* and align the bitstream! */
        R0 = inpos[0]|(inpos[1]<<8)|(inpos[2]<<16)|(inpos[3]<<24);inpos+=4;
        R1 = inpos[0]|(inpos[1]<<8)|(inpos[2]<<16)|(inpos[3]<<24);inpos+=4;
        R2 = inpos[0]|(inpos[1]<<8)|(inpos[2]<<16)|(inpos[3]<<24);inpos+=4;
        break;

      default:
        return DECR_ILLEGALDATA;
      }
    }

    /* buffer exhaustion check */
    if (inpos > endinp) {
      /* it's possible to have a file where the next run is less than
       * 16 bits in size. In this case, the READ_HUFFSYM() macro used
       * in building the tables will exhaust the buffer, so we should
       * allow for this, but not allow those accidentally read bits to
       * be used (so we check that there are at least 16 bits
       * remaining - in this boundary case they aren't really part of
       * the compressed data)
       */
      if (inpos > (endinp+2) || bitsleft < 16) return DECR_ILLEGALDATA;
    }

    while ((this_run = LZX(block_remaining)) > 0 && togo > 0) {
      if (this_run > togo) this_run = togo;
      togo -= this_run;
      LZX(block_remaining) -= this_run;

      /* apply 2^x-1 mask */
      window_posn &= window_size - 1;
      /* runs can't straddle the window wraparound */
      if ((window_posn + this_run) > window_size)
        return DECR_DATAFORMAT;

      switch (LZX(block_type)) {

      case LZX_BLOCKTYPE_VERBATIM:
        while (this_run > 0) {
          READ_HUFFSYM(MAINTREE, main_element);

          if (main_element < LZX_NUM_CHARS) {
            /* literal: 0 to LZX_NUM_CHARS-1 */
            window[window_posn++] = main_element;
            this_run--;
          }
          else {
            /* match: LZX_NUM_CHARS + ((slot<<3) | length_header (3 bits)) */
            main_element -= LZX_NUM_CHARS;
  
            match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
            if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
              READ_HUFFSYM(LENGTH, length_footer);
              match_length += length_footer;
            }
            match_length += LZX_MIN_MATCH;
  
            match_offset = main_element >> 3;
  
            if (match_offset > 2) {
              /* not repeated offset */
              if (match_offset != 3) {
                extra = extra_bits[match_offset];
                READ_BITS(verbatim_bits, extra);
                match_offset = lzx_position_base[match_offset] 
                               - 2 + verbatim_bits;
              }
              else {
                match_offset = 1;
              }
  
              /* update repeated offset LRU queue */
              R2 = R1; R1 = R0; R0 = match_offset;
            }
            else if (match_offset == 0) {
              match_offset = R0;
            }
            else if (match_offset == 1) {
              match_offset = R1;
              R1 = R0; R0 = match_offset;
            }
            else /* match_offset == 2 */ {
              match_offset = R2;
              R2 = R0; R0 = match_offset;
            }

            rundest = window + window_posn;
            this_run -= match_length;

            /* copy any wrapped around source data */
            if (window_posn >= match_offset) {
	      /* no wrap */
              runsrc = rundest - match_offset;
            } else {
              runsrc = rundest + (window_size - match_offset);
              copy_length = match_offset - window_posn;
              if (copy_length < match_length) {
                match_length -= copy_length;
                window_posn += copy_length;
                while (copy_length-- > 0) *rundest++ = *runsrc++;
                runsrc = window;
              }
            }
            window_posn += match_length;

            /* copy match data - no worries about destination wraps */
            while (match_length-- > 0) *rundest++ = *runsrc++;
          }
        }
        break;

      case LZX_BLOCKTYPE_ALIGNED:
        while (this_run > 0) {
          READ_HUFFSYM(MAINTREE, main_element);
  
          if (main_element < LZX_NUM_CHARS) {
            /* literal: 0 to LZX_NUM_CHARS-1 */
            window[window_posn++] = main_element;
            this_run--;
          }
          else {
            /* match: LZX_NUM_CHARS + ((slot<<3) | length_header (3 bits)) */
            main_element -= LZX_NUM_CHARS;
  
            match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
            if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
              READ_HUFFSYM(LENGTH, length_footer);
              match_length += length_footer;
            }
            match_length += LZX_MIN_MATCH;
  
            match_offset = main_element >> 3;
  
            if (match_offset > 2) {
              /* not repeated offset */
              extra = extra_bits[match_offset];
              match_offset = lzx_position_base[match_offset] - 2;
              if (extra > 3) {
                /* verbatim and aligned bits */
                extra -= 3;
                READ_BITS(verbatim_bits, extra);
                match_offset += (verbatim_bits << 3);
                READ_HUFFSYM(ALIGNED, aligned_bits);
                match_offset += aligned_bits;
              }
              else if (extra == 3) {
                /* aligned bits only */
                READ_HUFFSYM(ALIGNED, aligned_bits);
                match_offset += aligned_bits;
              }
              else if (extra > 0) { /* extra==1, extra==2 */
                /* verbatim bits only */
                READ_BITS(verbatim_bits, extra);
                match_offset += verbatim_bits;
              }
              else /* extra == 0 */ {
                /* ??? */
                match_offset = 1;
              }
  
              /* update repeated offset LRU queue */
              R2 = R1; R1 = R0; R0 = match_offset;
            }
            else if (match_offset == 0) {
              match_offset = R0;
            }
            else if (match_offset == 1) {
              match_offset = R1;
              R1 = R0; R0 = match_offset;
            }
            else /* match_offset == 2 */ {
              match_offset = R2;
              R2 = R0; R0 = match_offset;
            }

            rundest = window + window_posn;
            this_run -= match_length;

            /* copy any wrapped around source data */
            if (window_posn >= match_offset) {
	      /* no wrap */
              runsrc = rundest - match_offset;
            } else {
              runsrc = rundest + (window_size - match_offset);
              copy_length = match_offset - window_posn;
              if (copy_length < match_length) {
                match_length -= copy_length;
                window_posn += copy_length;
		while (copy_length-- > 0) *rundest++=*runsrc++;
                runsrc = window;
              }
            }
            window_posn += match_length;

            /* copy match data - no worries about destination wraps */
            while (match_length-- > 0) *rundest++ = *runsrc++;
          }
        }
        break;

      case LZX_BLOCKTYPE_UNCOMPRESSED:
        if ((inpos + this_run) > endinp) return DECR_ILLEGALDATA;
        memcpy(window + window_posn, inpos, (size_t) this_run);
        inpos += this_run; window_posn += this_run;
        break;

      default:
        return DECR_ILLEGALDATA; /* might as well */
      }

    }
  }

  if (togo != 0) return DECR_ILLEGALDATA;
  memcpy(CAB(outbuf), window + ((!window_posn) ? window_size : window_posn) -
    outlen, (size_t) outlen);

  CAB(window_posn) = window_posn;
  LZX(R0) = R0;
  LZX(R1) = R1;
  LZX(R2) = R2;

  /* intel E8 decoding */
  if ((LZX(frames_read)++ < 32768) && LZX(intel_filesize) != 0) {
    if (outlen <= 6 || !LZX(intel_started)) {
      LZX(intel_curpos) += outlen;
    }
    else {
      UBYTE *data    = CAB(outbuf);
      UBYTE *dataend = data + outlen - 10;
      LONG curpos    = LZX(intel_curpos);
      LONG filesize  = LZX(intel_filesize);
      LONG abs_off, rel_off;

      LZX(intel_curpos) = curpos + outlen;

      while (data < dataend) {
        if (*data++ != 0xE8) { curpos++; continue; }
        abs_off = data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
        if ((abs_off >= -curpos) && (abs_off < filesize)) {
          rel_off = (abs_off >= 0) ? abs_off - curpos : abs_off + filesize;
          data[0] = (UBYTE) rel_off;
          data[1] = (UBYTE) (rel_off >> 8);
          data[2] = (UBYTE) (rel_off >> 16);
          data[3] = (UBYTE) (rel_off >> 24);
        }
        data += 4;
        curpos += 5;
      }
    }
  }
  return DECR_OK;
}




/* all the file IO is abstracted into these routines:
 * cabinet_(open|close|read|seek|skip|getoffset)
 * file_(open|close|write)
 */

/* ensure_filepath("a/b/c/d.txt") ensures a, a/b and a/b/c exist as dirs */
int ensure_filepath(char *path) {
  struct stat st_buf;
  mode_t m;
  char *p;
  int ok;

  m = umask(0); umask(m); /* obtain user's umask */

  for (p = path; *p; p++) {
    if ((p != path) && (*p == '/')) {
      *p = 0;
      ok = (stat(path, &st_buf) == 0) && S_ISDIR(st_buf.st_mode);
      if (!ok) ok = (mkdir(path, 0777 & ~m) == 0);
      *p = '/';
      if (!ok) return 0;
    }
  }
  return 1;
}

#define FILENAME ".test.cabextract"

/* opens a file for output, returns success */
int file_open(struct file *fi) {
  /* create directories if needed, attempt to write file */
  if ((fi->fh = fopen(FILENAME, "wb"))) return 1;
  return 0;
}

/* closes a completed file, updates protections and timestamp */
void file_close(struct file *fi) {
  struct utimbuf utb;
  struct tm time;
  mode_t m;

  if (fi->fh) {
    fclose(fi->fh);
  }
  if ((fi->fh = fopen(FILENAME, "rb"))) {
    unsigned char buf[16];
    memset(buf, 0, 16);
    md5_stream (fi->fh, &buf[0]);
    fclose(fi->fh);
    printf("%02x%02x%02x%02x%02x%02x%02x%02x"
	   "%02x%02x%02x%02x%02x%02x%02x%02x %s\n",
	   buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6],
	   buf[7], buf[8], buf[9], buf[10], buf[11], buf[12],
	   buf[13], buf[14], buf[15], fi->filename);
  }
  unlink(FILENAME);
  fi->fh = NULL;
}

int file_write(struct file *fi, UBYTE *buf, size_t length) {
  if (fwrite((void *)buf, 1, length, fi->fh) != length) {
    perror(fi->realname);
    return 0;
  }
  return 1;
}

void cabinet_close(struct cabinet *cab) {
  if (cab->fh) {
    fclose(cab->fh);
  }
  cab->fh = NULL;
}

void cabinet_seek(struct cabinet *cab, off_t offset) {
  if (fseek(cab->fh, offset, SEEK_SET) < 0) {
    perror(cab->filename);
  }
}

void cabinet_skip(struct cabinet *cab, off_t distance) {
  if (fseek(cab->fh, distance, SEEK_CUR) < 0) {
    perror(cab->filename);
  }
}

off_t cabinet_getoffset(struct cabinet *cab) {
  return ftell(cab->fh);
}

/* read data from a cabinet, returns success */
int cabinet_read(struct cabinet *cab, UBYTE *buf, size_t length) {
  size_t avail = (size_t) (cab->filelen - cabinet_getoffset(cab));
  if (length > avail) {
    fprintf(stderr, "%s: WARNING; cabinet is truncated\n", cab->filename);
    length = avail;
  }
  if (fread((void *)buf, 1, length, cab->fh) != length) {
    perror(cab->filename);
    return 0;
  }
  return 1;
}

/* try to open a cabinet file, returns success */
int cabinet_open(struct cabinet *cab) {
  char *name = cab->filename;
  FILE *fh;

  /* note: this is now case sensitive */
  if (!(fh = fopen(name, "rb"))) {
    perror(name);
    return 0;
  }

  /* seek to end of file */
  if (fseek(fh, 0, SEEK_END) != 0) {
    perror(name);
    fclose(fh);
    return 0;
  }

  /* get length of file */
  cab->filelen = ftell(fh);

  /* return to the start of the file */
  if (fseek(fh, 0, SEEK_SET) != 0) {
    perror(name);
    fclose(fh);
    return 0;
  }

  cab->fh = fh;
  return 1;
}

/* allocate and read an aribitrarily long string from the cabinet */
char *cabinet_read_string(struct cabinet *cab) {
  off_t len=256, base = cabinet_getoffset(cab), maxlen = cab->filelen - base;
  int ok = 0, i;
  UBYTE *buf = NULL;
  do {
    if (len > maxlen) len = maxlen;
    if (!(buf = realloc(buf, (size_t) len))) break;
    if (!cabinet_read(cab, buf, (size_t) len)) break;

    /* search for a null terminator in what we've just read */
    for (i=0; i < len; i++) {
      if (!buf[i]) {ok=1; break;}
    }

    if (!ok) {
      if (len == maxlen) {
        fprintf(stderr, "%s: WARNING; cabinet is truncated\n", cab->filename);
        break;
      }
      len += 256;
      cabinet_seek(cab, base);
    }
  } while (!ok);

  if (!ok) {
    if (buf) free(buf); else fprintf(stderr, "out of memory!\n");
    return NULL;
  }

  /* otherwise, set the stream to just after the string and return */
  cabinet_seek(cab, base + ((off_t) strlen((char *) buf)) + 1);
  return (char *) buf;
}

/* reads the header and all folder and file entries in this cabinet */
int cabinet_read_entries(struct cabinet *cab) {
  int num_folders, num_files, header_resv, folder_resv = 0, i;
  struct folder *fol, *linkfol = NULL;
  struct file *file, *linkfile = NULL;
  off_t base_offset;
  UBYTE buf[64];

  /* read in the CFHEADER */
  base_offset = cabinet_getoffset(cab);
  if (!cabinet_read(cab, buf, cfhead_SIZEOF)) {
    return 0;
  }
  
  /* check basic MSCF signature */
  if (EndGetI32(buf+cfhead_Signature) != 0x4643534d) {
    fprintf(stderr, "%s: not a Microsoft cabinet file\n", cab->filename);
    return 0;
  }

  /* get the number of folders */
  num_folders = EndGetI16(buf+cfhead_NumFolders);
  if (num_folders == 0) {
    fprintf(stderr, "%s: no folders in cabinet\n", cab->filename);
    return 0;
  }

  /* get the number of files */
  num_files = EndGetI16(buf+cfhead_NumFiles);
  if (num_files == 0) {
    fprintf(stderr, "%s: no files in cabinet\n", cab->filename);
    return 0;
  }

  /* just check the header revision */
  if ((buf[cfhead_MajorVersion] > 1) ||
      (buf[cfhead_MajorVersion] == 1 && buf[cfhead_MinorVersion] > 3))
  {
    fprintf(stderr, "%s: WARNING; cabinet format version > 1.3\n",
	    cab->filename);
  }

  /* read the reserved-sizes part of header, if present */
  cab->flags = EndGetI16(buf+cfhead_Flags);
  if (cab->flags & cfheadRESERVE_PRESENT) {
    if (!cabinet_read(cab, buf, cfheadext_SIZEOF)) return 0;
    header_resv     = EndGetI16(buf+cfheadext_HeaderReserved);
    folder_resv     = buf[cfheadext_FolderReserved];
    cab->block_resv = buf[cfheadext_DataReserved];

    if (header_resv > 60000) {
      fprintf(stderr, "%s: WARNING; header reserved space > 60000\n",
	      cab->filename);
    }

    /* skip the reserved header */
    if (header_resv) fseek(cab->fh, (off_t) header_resv, SEEK_CUR);
  }

  if (cab->flags & cfheadPREV_CABINET) {
    cab->prevname = cabinet_read_string(cab);
    if (!cab->prevname) return 0;
    cab->previnfo = cabinet_read_string(cab);
  }

  if (cab->flags & cfheadNEXT_CABINET) {
    cab->nextname = cabinet_read_string(cab);
    if (!cab->nextname) return 0;
    cab->nextinfo = cabinet_read_string(cab);
  }

  /* read folders */
  for (i = 0; i < num_folders; i++) {
    if (!cabinet_read(cab, buf, cffold_SIZEOF)) return 0;
    if (folder_resv) cabinet_skip(cab, folder_resv);

    fol = (struct folder *) calloc(1, sizeof(struct folder));
    if (!fol) { fprintf(stderr, "out of memory!\n"); return 0; }

    fol->cab[0]     = cab;
    fol->offset[0]  = base_offset + (off_t) EndGetI32(buf+cffold_DataOffset);
    fol->num_blocks = EndGetI16(buf+cffold_NumBlocks);
    fol->comp_type  = EndGetI16(buf+cffold_CompType);

    if (!linkfol) cab->folders = fol; else linkfol->next = fol;
    linkfol = fol;
  }

  /* read files */
  for (i = 0; i < num_files; i++) {
    if (!cabinet_read(cab, buf, cffile_SIZEOF)) return 0;
    file = (struct file *) calloc(1, sizeof(struct file));
    if (!file) { fprintf(stderr, "out of memory!\n"); return 0; }
      
    file->length   = EndGetI32(buf+cffile_UncompressedSize);
    file->offset   = EndGetI32(buf+cffile_FolderOffset);
    file->index    = EndGetI16(buf+cffile_FolderIndex);
    file->time     = EndGetI16(buf+cffile_Time);
    file->date     = EndGetI16(buf+cffile_Date);
    file->attribs  = EndGetI16(buf+cffile_Attribs);
    file->filename = cabinet_read_string(cab);
    if (!file->filename) return 0;
    if (!linkfile) cab->files = file; else linkfile->next = file;
    linkfile = file;
  }
  return 1;
}


/* this does the tricky job of running through every file in the cabinet,
 * including spanning cabinets, and working out which file is in which
 * folder in which cabinet. It also throws out the duplicate file entries
 * that appear in spanning cabinets. There is memory leakage here because
 * those entries are not freed. See the XAD CAB client for an
 * implementation of this that correctly frees the discarded file entries.
 */
struct file *process_files(struct cabinet *basecab) {
  struct cabinet *cab;
  struct file *outfi = NULL, *linkfi = NULL, *nextfi, *fi, *cfi;
  struct folder *fol, *firstfol, *lastfol = NULL, *predfol;
  int i, mergeok;

  for (cab = basecab; cab; cab = cab->nextcab) {
    /* firstfol = first folder in this cabinet */
    /* lastfol  = last folder in this cabinet */
    /* predfol  = last folder in previous cabinet (or NULL if first cabinet) */
    predfol = lastfol;
    firstfol = cab->folders;
    for (lastfol = firstfol; lastfol->next;) lastfol = lastfol->next;
    mergeok = 1;

    for (fi = cab->files; fi; fi = nextfi) {
      i = fi->index;
      nextfi = fi->next;

      if (i < cffileCONTINUED_FROM_PREV) {
        for (fol = firstfol; fol && i--; ) fol = fol->next;
        fi->folder = fol; /* NULL if an invalid folder index */
      }
      else {
        /* folder merging */
        if (i == cffileCONTINUED_TO_NEXT
        ||  i == cffileCONTINUED_PREV_AND_NEXT) {
          if (cab->nextcab && !lastfol->contfile) lastfol->contfile = fi;
        }

        if (i == cffileCONTINUED_FROM_PREV
        ||  i == cffileCONTINUED_PREV_AND_NEXT) {
          /* these files are to be continued in yet another
           * cabinet, don't merge them in just yet */
          if (i == cffileCONTINUED_PREV_AND_NEXT) mergeok = 0;

          /* only merge once per cabinet */
          if (predfol) {
            if ((cfi = predfol->contfile)
            && (cfi->offset == fi->offset)
            && (cfi->length == fi->length)
            && (strcmp(cfi->filename, fi->filename) == 0)
            && (predfol->comp_type == firstfol->comp_type)) {
              /* increase the number of splits */
              if ((i = ++(predfol->num_splits)) > CAB_SPLITMAX) {
                mergeok = 0;
                fprintf(stderr, "%s: internal error, increase CAB_SPLITMAX\n",
                  basecab->filename);
              }
              else {
                /* copy information across from the merged folder */
                predfol->offset[i] = firstfol->offset[0];
                predfol->cab[i]    = firstfol->cab[0];
                predfol->next      = firstfol->next;
                predfol->contfile  = firstfol->contfile;

                if (firstfol == lastfol) lastfol = predfol;
                firstfol = predfol;
                predfol = NULL; /* don't merge again within this cabinet */
              }
            }
            else {
              /* if the folders won't merge, don't add their files */
              mergeok = 0;
            }
          }

          if (mergeok) fi->folder = firstfol;
        }
      }

      if (fi->folder) {
        if (linkfi) linkfi->next = fi; else outfi = fi;
        linkfi = fi;
      }
    } /* for (fi= .. */
  } /* for (cab= ...*/

  /* terminate the list */
  if (linkfi) linkfi->next = NULL;

  return outfi;
}

/* validates and reads file entries from a cabinet at offset [offset] in
 * file [name]. Returns a cabinet structure if successful, or NULL
 * otherwise.
 */
struct cabinet *load_cab_offset(char *name, off_t offset) {
  struct cabinet *cab = (struct cabinet *) calloc(1, sizeof(struct cabinet));
  int ok;
  if (!cab) return NULL;

  cab->filename = name;
  if ((ok = cabinet_open(cab))) {
    cabinet_seek(cab, offset);
    ok = cabinet_read_entries(cab);
    cabinet_close(cab);
  }

  if (ok) return cab;
  free(cab);
  return NULL;
}

/* Searches a file for embedded cabinets (also succeeds on just normal
 * cabinet files). The first result of this search will be returned, and
 * the remaining results will be chained to it via the cab->next structure
 * member.
 */
#define SEARCH_SIZE (32*1024)
UBYTE search_buf[SEARCH_SIZE];

struct cabinet *find_cabs_in_file(char *name) {
  struct cabinet *cab, *cab2, *firstcab = NULL, *linkcab = NULL;
  UBYTE *pstart = &search_buf[0], *pend, *p;
  ULONG offset, caboff, cablen, foffset, filelen;
  size_t length;
  int state = 0, found = 0, ok = 0, filefound = 0;

  /* open the file and search for cabinet headers */
  if ((cab = (struct cabinet *) calloc(1, sizeof(struct cabinet)))) {
    cab->filename = name;
    if (cabinet_open(cab)) {
      filefound = 1;
      filelen = (ULONG) cab->filelen;
      for (offset = 0; offset < filelen; offset += length) {
	/* search length is either the full length of the search buffer,
	 * or the amount of data remaining to the end of the file,
	 * whichever is less.
	 */
	length = filelen - offset;
	if (length > SEARCH_SIZE) length = SEARCH_SIZE;

	/* fill the search buffer with data from disk */
	if (!cabinet_read(cab, search_buf, length)) break;

	/* check for InstallShield '.cab' header, "ISc(" */
	if ((offset == 0) && EndGetI32(&search_buf[0]) == 0x28635349) {
	  fprintf(stderr, "%s: WARNING; found InstallShield header.\n", name);
	  fprintf(stderr, "%s:          This is probably an InstallShield\n",
		  name);
	  fprintf(stderr, "%s:          file. Use the tool I5COMP or I6COMP\n",
		  name);
	  fprintf(stderr, "%s:          to unpack this file format.\n", name);
	}

	/* read through the entire buffer. */
	p = pstart;
	pend = &search_buf[length];
	while (p < pend) {
	  switch (state) {
	  /* starting state */
	  case 0:
	    /* we spend most of our time in this while loop, looking for
	     * a leading 'M' of the 'MSCF' signature
	     */
	    while (*p++ != 0x4D && p < pend);
	    if (p < pend) state = 1; /* if we found tht 'M', advance state */
	    break;

	  /* verify that the next 3 bytes are 'S', 'C' and 'F' */
	  case 1: state = (*p++ == 0x53) ? 2 : 0; break;
	  case 2: state = (*p++ == 0x43) ? 3 : 0; break;
	  case 3: state = (*p++ == 0x46) ? 4 : 0; break;

	  /* we don't care about bytes 4-7 */
	  /* bytes 8-11 are the overall length of the cabinet */
	  case 8:  cablen  = *p++;       state++; break;
	  case 9:  cablen |= *p++ << 8;  state++; break;
	  case 10: cablen |= *p++ << 16; state++; break;
	  case 11: cablen |= *p++ << 24; state++; break;

	  /* we don't care about bytes 12-15 */
	  /* bytes 16-19 are the offset within the cabinet of the filedata */
	  case 16: foffset  = *p++;       state++; break;
	  case 17: foffset |= *p++ << 8;  state++; break;
	  case 18: foffset |= *p++ << 16; state++; break;
	  case 19: foffset |= *p++ << 24;
	    /* now we have recieved 20 bytes of potential cab header. */
	    /* work out the offset in the file of this potential cabinet */
	    caboff = offset + (p-pstart) - 20;

	    /* check that the files offset is less than the alleged length
	     * of the cabinet, and that the offset + the alleged length are
	     * 'roughly' within the end of overall file length
	     */
	    if ((foffset < cablen) &&
		((caboff + foffset) < (filelen + 32)) &&
		((caboff + cablen) < (filelen + 32)) )
	    {
	      /* found a potential result - try loading it */
	      found++;
	      cab2 = load_cab_offset(name, (off_t) caboff);
	      if (cab2) {
		/* success */
		ok++;

		/* cause the search to restart after this cab's data. */
		offset = caboff + cablen;
		if (offset < cab->filelen) cabinet_seek(cab, offset);
		length = 0;
		p = pend;

		/* link the cab into the list */
		if (linkcab == NULL) firstcab = cab2;
		else linkcab->next = cab2;
		linkcab = cab2;
	      }
	    }
	    state = 0;
	    break;
	  default:
	    p++, state++; break;
	  }
	}
      }
      cabinet_close(cab);
    }
    free(cab);
  }

  /* if there were cabinets that were found but are not ok, point this out */
  if (found > ok) {
    fprintf(stderr, "%s: WARNING; found %d bad cabinets\n", name, found-ok);
  }

  /* if a file was found, but no cabinets were found, let the user know */
  if (filefound && (!firstcab)) {
    fprintf(stderr, "%s: not a Microsoft cabinet file.\n", name);
  }
  return firstcab;
}

/* UTF translates two-byte unicode characters into 1, 2 or 3 bytes.
 * %000000000xxxxxxx -> %0xxxxxxx
 * %00000xxxxxyyyyyy -> %110xxxxx %10yyyyyy
 * %xxxxyyyyyyzzzzzz -> %1110xxxx %10yyyyyy %10zzzzzz
 *
 * Therefore, the inverse is as follows:
 * First char:
 *  0x00 - 0x7F = one byte char
 *  0x80 - 0xBF = invalid
 *  0xC0 - 0xDF = 2 byte char (next char only 0x80-0xBF is valid)
 *  0xE0 - 0xEF = 3 byte char (next 2 chars only 0x80-0xBF is valid)
 *  0xF0 - 0xFF = invalid
 */

/* translate UTF -> ASCII */
int convertUTF(UBYTE *in) {
  UBYTE c, *out = in, *end = in + strlen((char *) in) + 1;
  ULONG x;

  do {
    /* read unicode character */
    if ((c = *in++) < 0x80) x = c;
    else {
      if (c < 0xC0) return 0;
      else if (c < 0xE0) {
        x = (c & 0x1F) << 6;
        if ((c = *in++) < 0x80 || c > 0xBF) return 0; else x |= (c & 0x3F);
      }
      else if (c < 0xF0) {
        x = (c & 0xF) << 12;
        if ((c = *in++) < 0x80 || c > 0xBF) return 0; else x |= (c & 0x3F)<<6;
        if ((c = *in++) < 0x80 || c > 0xBF) return 0; else x |= (c & 0x3F);
      }
      else return 0;
    }

    /* terrible unicode -> ASCII conversion */
    if (x > 127) x = '_';

    if (in > end) return 0; /* just in case */
  } while ((*out++ = (UBYTE) x));
  return 1;
}

void print_fileinfo(struct file *fi) {
  int d = fi->date, t = fi->time;
  char *fname = NULL;

  if (fi->attribs & cffile_A_NAME_IS_UTF) {
    fname = malloc(strlen(fi->filename) + 1);
    if (fname) {
      strcpy(fname, fi->filename);
      convertUTF((UBYTE *) fname);
    }
  }

  printf("%9u | %02d.%02d.%04d %02d:%02d:%02d | %s\n",
    fi->length, 
    d & 0x1f, (d>>5) & 0xf, (d>>9) + 1980,
    t >> 11, (t>>5) & 0x3f, (t << 1) & 0x3e,
    fname ? fname : fi->filename
  );

  if (fname) free(fname);
}

int NONEdecompress(int inlen, int outlen) {
  if (inlen != outlen) return DECR_ILLEGALDATA;
  memcpy(CAB(outbuf), CAB(inbuf), (size_t) inlen);
  return DECR_OK;
}

ULONG checksum(UBYTE *data, UWORD bytes, ULONG csum) {
  int len;
  ULONG ul = 0;

  for (len = bytes >> 2; len--; data += 4) {
    csum ^= ((data[0]) | (data[1]<<8) | (data[2]<<16) | (data[3]<<24));
  }

  switch (bytes & 3) {
  case 3: ul |= *data++ << 16;
  case 2: ul |= *data++ <<  8;
  case 1: ul |= *data;
  }
  csum ^= ul;

  return csum;
}

int decompress(struct file *fi, int savemode, int fix) {
  ULONG bytes = savemode ? fi->length : fi->offset - CAB(offset);
  struct cabinet *cab = CAB(current)->cab[CAB(split)];
  UBYTE buf[cfdata_SIZEOF], *data;
  UWORD inlen, len, outlen, cando;
  ULONG cksum;
  LONG err;

  while (bytes > 0) {
    /* cando = the max number of bytes we can do */
    cando = CAB(outlen);
    if (cando > bytes) cando = bytes;

    /* if cando != 0 */
    if (cando && savemode) file_write(fi, CAB(outpos), cando);

    CAB(outpos) += cando;
    CAB(outlen) -= cando;
    bytes -= cando; if (!bytes) break;

    /* we only get here if we emptied the output buffer */

    /* read data header + data */
    inlen = outlen = 0;
    while (outlen == 0) {
      /* read the block header, skip the reserved part */
      if (!cabinet_read(cab, buf, cfdata_SIZEOF)) return DECR_INPUT;
      cabinet_skip(cab, cab->block_resv);

      /* we shouldn't get blocks over CAB_INPUTMAX in size */
      data = CAB(inbuf) + inlen;
      len = EndGetI16(buf+cfdata_CompressedSize);
      inlen += len;
      if (inlen > CAB_INPUTMAX) return DECR_INPUT;
      if (!cabinet_read(cab, data, len)) return DECR_INPUT;

      /* clear two bytes after read-in data */
      data[len+1] = data[len+2] = 0;

      /* perform checksum test on the block (if one is stored) */
      cksum = EndGetI32(buf+cfdata_CheckSum);
      if (cksum && cksum != checksum(buf+4, 4, checksum(data, len, 0))) {
	/* checksum is wrong */
	if (fix && ((fi->folder->comp_type & cffoldCOMPTYPE_MASK)
		    == cffoldCOMPTYPE_MSZIP))
        {
	  fprintf(stderr, "%s: WARNING; checksum failed\n", fi->filename); 
	}
	else {
	  return DECR_CHECKSUM;
	}
      }

      /* outlen=0 means this block was part of a split block */
      outlen = EndGetI16(buf+cfdata_UncompressedSize);
      if (outlen == 0) {
        cabinet_close(cab);
        cab = CAB(current)->cab[++CAB(split)];
        if (!cabinet_open(cab)) return DECR_INPUT;
        cabinet_seek(cab, CAB(current)->offset[CAB(split)]);
      }
    }

    /* decompress block */
    if ((err = CAB(decompress)(inlen, outlen))) {
      if (fix && ((fi->folder->comp_type & cffoldCOMPTYPE_MASK)
		  == cffoldCOMPTYPE_MSZIP))
      {
	fprintf(stderr, "%s: WARNING; failed decrunching block\n",
		fi->filename); 
      }
      else {
	return err;
      }
    }
    CAB(outlen) = outlen;
    CAB(outpos) = CAB(outbuf);
  }

  return DECR_OK;
}


void extract_file(struct file *fi, int fix) {
  struct folder *fol = fi->folder, *oldfol = CAB(current);
  LONG err = DECR_OK;

  /* is a change of folder needed? do we need to reset the current folder? */
  if (fol != oldfol || fi->offset < CAB(offset)) {
    UWORD comptype = fol->comp_type;

    /* if the archiver has changed, call the old archiver's free() function */
    switch (comptype & cffoldCOMPTYPE_MASK) {
    case cffoldCOMPTYPE_NONE:
      CAB(decompress) = NONEdecompress;
      break;

    case cffoldCOMPTYPE_MSZIP:
      if (CAB(window) != NULL) {
	free((void *) CAB(window));
	CAB(window) = NULL;
	CAB(actual_size) = 0;
      }
      CAB(decompress) = ZIPdecompress;
      break;

    case cffoldCOMPTYPE_QUANTUM:
      CAB(decompress) = QTMdecompress;
      err = QTMinit((comptype >> 8) & 0x1f, (comptype >> 4) & 0xF);
      break;

    case cffoldCOMPTYPE_LZX:
      CAB(decompress) = LZXdecompress;
      err = LZXinit((comptype >> 8) & 0x1f);
      break;

    default:
      err = DECR_DATAFORMAT;
    }
    if (err) goto exit_handler;

    /* initialisation OK, set current folder and reset offset */
    if (oldfol) cabinet_close(oldfol->cab[CAB(split)]);
    if (!cabinet_open(fol->cab[0])) goto exit_handler;
    cabinet_seek(fol->cab[0], fol->offset[0]);
    CAB(current) = fol;
    CAB(offset) = 0;
    CAB(outlen) = 0; /* discard existing block */
    CAB(split)  = 0;
  }

  if (fi->offset > CAB(offset)) {
    /* decode bytes and send them to /dev/null */
    if ((err = decompress(fi, 0, fix))) goto exit_handler;
    CAB(offset) = fi->offset;
  }
  if (!file_open(fi)) return;
  err = decompress(fi, 1, fix);
  if (err) CAB(current) = NULL; else CAB(offset) += fi->length;
  file_close(fi);

exit_handler:
  if (err) {
    char *errmsg, *cabname;
    switch (err) {
    case DECR_NOMEMORY:
      errmsg = "out of memory!\n"; break;
    case DECR_ILLEGALDATA:
      errmsg = "%s: illegal or corrupt data\n"; break;
    case DECR_DATAFORMAT:
      errmsg = "%s: unsupported data format\n"; break;
    case DECR_CHECKSUM:
      errmsg = "%s: checksum error\n"; break;
    case DECR_INPUT:
      errmsg = "%s: input error\n"; break;
    case DECR_OUTPUT:
      errmsg = "%s: output error\n"; break;
    default:
      errmsg = "%s: unknown error (BUG)\n";
    }

    if (CAB(current)) {
      cabname = CAB(current)->cab[CAB(split)]->filename;
    }
    else {
      cabname = fi->folder->cab[0]->filename;
    }

    fprintf(stderr, errmsg, cabname);
  }
}

/* returns 1 if it's determined that the file-list filenames are using
 * unix directory seperators ("/") instead of MS-DOS directory seperators
 * ("\")
 *
 * The algorithm is pretty much guesswork, but it's generally OK. Just
 * don't give it cabs with no directories and several files with dates
 * in them, eg "Report 20/01/02.doc" :)
 *
 * - First, look at all the slashes in all the filenames.
 *   - if there are no slashe, MS-DOS seperators is assumed (doesn't matter!)
 *   - if they are all backslashes, MS-DOS seperators is assumed.
 *   - if they are all forward slashes, UNIX seperators is assumed
 *
 * - we now know there are both forward and backward slashes.
 * - So, we go through each filename, looking for the first slash.
 *   - if the part of the filename up to and including the slash matches
 *     the same
 *
 * Any suggestions for improvement to this algorithm (or cabs which defy it)
 * are welcome.
 */
int unix_directory_seperators(struct file *files) {
  struct file *fi;
  char slsh=0, bkslsh=0, *oldname;
  int oldlen;

  for (fi = files; fi; fi = fi->next) {
    char *p;
    for (p = fi->filename; *p; p++) {
      if (*p == '/') slsh = 1;
      if (*p == '\\') bkslsh = 1;
    }
    if (slsh && bkslsh) break;
  }

  if (slsh) {
    if (!bkslsh) return 1; /* slashes, but no backslashes. UNIX */
  }
  else {
    /* no slashes, and either backslashes or no backslashes. MS-DOS */
    return 0;
  }

  /* special case if there's only one file - just take the first slash */
  if (! files->next) {
    char c, *p = fi->filename;
    while (c = *p++) {
      if (c == '\\') return 0;
      if (c == '/')  return 1;
    }
    /* should not happen - we counted at least one slash! */
    return 0;
  }

  oldname = NULL;
  oldlen = 0;
  for (fi = files; fi; fi=fi->next) {
    char *name = fi->filename;
    int len = 0;
    while (name[len]) {
      if ((name[len] == '\\') || (name[len] == '/')) break;
      len++;
    }
    if (!name[len]) len = 0; else len++;

    if (len && (len == oldlen)) {
      if (strncmp(name, oldname, len) == 0)
	return (name[len-1] == '\\') ? 0 : 1;
    }
    
    oldname = name;
    oldlen = len;
  }

  /* default */
  return 0;
}

/* fills in realname for each file in the cabinet */
char *create_output_name(char *fname, char *dir, int lower, int isunix) {
  char sep   = (isunix) ? '/'  : '\\'; /* the path-seperator */
  char slash = (isunix) ? '\\' : '/';  /* the other slash */
  char c, *p, *name;

  if (!(name = malloc(strlen(fname) + (dir ? strlen(dir) : 0) + 2))) {
    fprintf(stderr, "out of memory!\n");
    return NULL;
  }
  
  /* start with blank name */
  *name = 0;

  /* add output directory if needed */
  if (dir) {
    strcpy(name, dir);
    strcat(name, "/");
  }

  /* remove leading slashes */
  while (*fname == sep) fname++;

  /* copy from fi->filename to new name, converting MS-DOS slashes to UNIX
   * slashes as we go. Also lowercases characters if needed.
   */
  p = &name[strlen(name)];
  do {
    c = *fname++;
    *p++ = (c == slash) ? sep :
      (    (c == sep)   ? slash :
	   (lower ? tolower((unsigned char) c) : c));
  } while (c);

  return name;
}

/* This is a system of remembering all the cabinets extracted in an
 * invocation of cabextract, so even silly people who don't read the
 * documentation will not have to see their chained cabinets extracted a
 * great number of times.
 */
struct cab_remember {
  struct cab_remember *next;
  dev_t st_dev;
  ino_t st_ino; 
  char *from;
};

static struct cab_remember *remember_list = NULL;

/* adds a known cabinet name to the list */
void remember_cabinet(char *name, char *from) {
  struct stat st_buf;
  struct cab_remember *cr;
  if (stat(name, &st_buf) == 0) {
    if ((cr = calloc(1, sizeof(struct cab_remember)))) {
      cr->st_dev = st_buf.st_dev;
      cr->st_ino = st_buf.st_ino;
      if (from && (cr->from = malloc(strlen(from)+1))) strcpy(cr->from, from);
      cr->next = remember_list;
      remember_list = cr;
    }
  }
}

/* returns non-zero if a cabinet has previously been extracted. If there
 * is a 'from cabinet', that will be placed in *from, otherwise NULL will
 * be placed there.
 */
int known_cabinet(char *name, char **from) {
  struct cab_remember *cr;
  struct stat st_buf;

  if (stat(name, &st_buf) != 0) return 0;

  for (cr = remember_list; cr != NULL; cr = cr->next) {
    if ((st_buf.st_ino == cr->st_ino) && (st_buf.st_dev == cr->st_dev)) {
      if (from) *from = cr->from;
      return 1;
    }
  }
  if (from) *from = NULL;
  return 0;
}

/* tries to find *cabname, from the directory path of origcab, correcting the
 * case of *cabname if necessary, If found, writes back to *cabname.
 */
void find_cabinet_file(char **cabname, char *origcab) {
  char *tail, *cab, *name, *nextpart;
  struct dirent *entry;
  struct stat st_buf;
  int found = 0, len;
  DIR *dir;

  /* ensure we have a cabinet name at all */
  if (!(name = *cabname)) return;

  /* find if there's a directory path in the origcab */
  tail = origcab ? strrchr(origcab, '/') : NULL;

  if ((cab = (char *) malloc((tail ? tail-origcab : 1) + strlen(name) + 2))) {
    /* add the directory path from the original cabinet name */
    if (tail) {
      memcpy(cab, origcab, tail-origcab);
      cab[tail-origcab] = '\0';
    }
    else {
      /* default directory path of '.' */
      cab[0] = '.';
      cab[1] = '\0';
    }

    do {
      /* we don't want null cabinet filenames */
      if (name[0] == '\0') break;

      /* if there is a directory component in the cabinet name,
       * look for that alone first
       */
      nextpart = strchr(name, '\\');
      if (nextpart) *nextpart = '\0';

      /* try accessing the component with its current name (case-sensitive) */
      len = strlen(cab); strcat(cab, "/"); strcat(cab, name);
      found = (stat(cab, &st_buf) == 0) && 
	(nextpart ? S_ISDIR(st_buf.st_mode) : S_ISREG(st_buf.st_mode));

      /* if the component was not found, look for it in the current dir */
      if (!found) {
	cab[len] = '\0';

	if ((dir = opendir(cab))) {
	  while ((entry = readdir(dir))) {
	    if (strcasecmp(name, entry->d_name) == 0) {
	      strcat(cab, "/"); strcat(cab, entry->d_name); found = 1; break;
	    }
	  }
	  closedir(dir);
	}
      }
      
      /* restore the real name and skip to the next directory component
       * or actual cabinet name
       */
      if (nextpart) *nextpart = '\\', name = &nextpart[1];

      /* while there is another directory component, and while we
       * successfully found the current component
       */
    } while (nextpart && found);

    /* if we found the cabinet, change the next cabinet's name.
     * otherwise, pretend nothing happened
     */
    if (found) {
      free((void *) *cabname);
      *cabname = cab;
    }
    else {
      free((void *) cab);
    }
  }
}

/* process_cabinet() is called by main() for every file listed on the
 * command line. It will find every cabinet file in that file, and will
 * search for every chained cabinet attached to those cabinets, then it
 * will either extract or list the cabinet(s). Returns 0 for success or 1
 * for failure (unlike most cabextract functions).
 */
int process_cabinet(char *cabname, char *dir,
		    int fix, int view, int lower, int quiet) {
 
  struct cabinet *basecab, *cab, *cab1, *cab2;
  struct file *filelist, *fi;
  char *from;
  int isunix;

  /* has the list-mode header been seen before? */
  int viewhdr = 0;

  /* do not process repeat cabinets */
  if (known_cabinet(cabname, &from)) {
    if (!quiet) {
      if (from) {
	printf("%s: skipping known cabinet (from %s)\n", cabname, from);
      }
      else {
	printf("%s: skipping known cabinet\n", cabname);
      }
    }
    return 0; /* return success */
  }
  remember_cabinet(cabname, NULL);

  /* load the file requested */
  basecab = find_cabs_in_file(cabname);
  if (!basecab) return 1;

  if (view || !quiet) {
    printf("%s cabinet: %s\n", view ? "Viewing" : "Extracting", cabname);
  }

  printf("*** %s\n", cabname);
  fflush(stdout);

  /* iterate over all cabinets found in that file */
  for (cab = basecab; cab; cab=cab->next) {

    /* bi-directionally load any spanning cabinets -- backwards */
    for (cab1 = cab; cab1->flags & cfheadPREV_CABINET; cab1 = cab1->prevcab) {
      if (!quiet) printf("%s: extends backwards to %s (%s)\n", cabname,
			 cab1->prevname, cab1->previnfo);
      find_cabinet_file(&(cab1->prevname), cabname);
      if (!(cab1->prevcab = load_cab_offset(cab1->prevname, 0))) {
        fprintf(stderr, "%s: can't read previous cabinet %s\n",
		cabname, cab1->prevname);
        break;
      }
      remember_cabinet(cab1->prevname, cabname);
      cab1->prevcab->nextcab = cab1;
    }

    /* bi-directionally load any spanning cabinets -- forwards */
    for (cab2 = cab; cab2->flags & cfheadNEXT_CABINET; cab2 = cab2->nextcab) {
      if (!quiet) printf("%s: extends to %s (%s)\n", cabname,
			 cab2->nextname, cab2->nextinfo);
      find_cabinet_file(&(cab2->nextname), cabname);
      if (!(cab2->nextcab = load_cab_offset(cab2->nextname, 0))) {
        fprintf(stderr, "%s: can't read next cabinet %s\n",
		cabname, cab2->nextname);
        break;
      }
      remember_cabinet(cab2->nextname, cabname);
      cab2->nextcab->prevcab = cab2;
    }

    /* remove any duplicate file entries. (duplicate file entries appear
     * in both parts of a split cabinet)
     */
    filelist = process_files(cab1);

    /* work out if the files use UNIX directory seperators instead of the
     * usual MS-DOS directory seperators
     */
    isunix = unix_directory_seperators(filelist);

    CAB(current) = NULL;

    if (view && !viewhdr) {
      printf("File size | Date       Time     | Name\n");
      printf("----------+---------------------+-------------\n");
      viewhdr = 1;
    }
    for (fi = filelist; fi; fi = fi->next) {
      if (view) {
	print_fileinfo(fi);
      }
      else {
	char *realname = create_output_name(fi->filename, dir, lower, isunix);
	if (!realname) return 1;
	fi->realname = realname;
	if (!quiet) printf("  extracting: %s\n", realname);
	extract_file(fi, fix);
	free(realname);
	fi->realname = NULL;
      }
    }
  }

  if (view) printf("\n");
  else if (!quiet) printf("Finished processing cabinet.\n\n");
  return 0;
}


struct option opts[] = {
  { "directory", 1, NULL, 'd' },
  { "fix",       0, NULL, 'f' },
  { "filter",    1, NULL, 'F' },
  { "help",      0, NULL, 'h' },
  { "list",      0, NULL, 'l' },
  { "lowercase", 0, NULL, 'L' },
  { "pipe",      0, NULL, 'p' },
  { "quiet",     0, NULL, 'q' },
  { "single",    0, NULL, 's' },
  { "version",   0, NULL, 'v' },
  { NULL,        0, NULL, 0   }
};

int main(int argc, char *argv[]) {
  int help=0, list=0, lower=0, view=0, quiet=1, fix=0, x, err=0;
  char *dir = NULL;

  memset(&decomp_state, 0, sizeof(decomp_state));

  while ((x = getopt_long(argc, argv, "d:fF:hlLpqsv", opts, NULL)) != -1) {
    switch (x) {
    case 'v': view  = 1;      break;
    case 'h': help  = 1;      break;
    case 'l': list  = 1;      break;
    case 'q': quiet = 1;      break;
    case 'L': lower = 1;      break;
    case 'f': fix   = 1;      break;
    case 'd': dir   = optarg; break;
    }
  }

  if (help) {
    fprintf(stderr,
      "Usage: %s [options] [-d dir] <cabinet file(s)>\n\n"
      "This will extract all files from a cabinet or executable cabinet.\n"
      "For multi-part cabinets, only specify the first file in the set.\n\n"
      "Options:\n"
      "  -v   --version     print version / list cabinet\n"
      "  -h   --help        show this help page\n"
      "  -l   --list        list contents of cabinet\n"
      "  -q   --quiet       only print errors and warnings\n"
      "  -L   --lowercase   make filenames lowercase\n"
      "  -f   --fix         fix (some) corrupted cabinets\n"
      "  -n   --nosearch    limit cab-search to files on the command line\n"
      "  -d   --directory   extract all files to the given directory\n\n"
      "cabextract %s (C) 2000-2002 Stuart Caie <kyzer@4u.net>\n"
      "This is free software with ABSOLUTELY NO WARRANTY.\n",
      argv[0], VERSION
    );
    return 1;
  }

  if (optind == argc) {
    /* no arguments other than the options */
    if (view) {
      printf("cabextract version %s\n", VERSION);
      return 0;
    }
    else {
      fprintf(stderr, "cabextract: No cabinet files specified.\n"
	              "Try '%s --help' for more information.\n", argv[0]);
      return 1;
    }
  }

  while (optind != argc) {
    err |= process_cabinet(argv[optind++], dir, fix, view||list, lower, quiet);
  }

  return err;
}
