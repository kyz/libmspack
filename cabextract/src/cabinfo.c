/* cabinfo -- dumps useful information from cabinets
 * (C) 2000-2018 Stuart Caie <kyzer@cabextract.org.uk>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STRINGS_H
# include <strings.h>
#endif

/* include <system.h> from libmspack for LD and EndGetI?? macros */
#include <system.h>
/* include <cab.h> from libmspack for cab structure offsets */
#include <cab.h>

#if HAVE_FSEEKO
# define FSEEK fseeko
# define FTELL ftello
# define FILELEN off_t
#else
# define FSEEK fseek
# define FTELL ftell
# define FILELEN long
#endif

FILE *fh;
char *filename;
FILELEN filelen;
void search();
void getinfo();

#define GETLONG(n) EndGetI32(&buf[n])
#define GETWORD(n) EndGetI16(&buf[n])
#define GETBYTE(n) ((int)buf[n])

#define GETOFFSET      (FTELL(fh))
#define READ(buf,len)  if (myread((void *)(buf),(len))) return
#define SKIP(offset)   if (myseek((offset),SEEK_CUR)) return
#define SEEK(offset)   if (myseek((offset),SEEK_SET)) return

int myread(void *buf, int length) {
  FILELEN remain = filelen - GETOFFSET;
  if (length > remain) length = (int) remain;
  if (fread(buf, 1, length, fh) != (size_t) length) {
    perror(filename);
    return 1;
  }
  return 0;
}

int myseek(FILELEN offset, int mode) {
  if (FSEEK(fh, offset, mode) != 0) {
    perror(filename);
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  int i;

  printf("Cabinet information dumper by Stuart Caie <kyzer@cabextract.org.uk>\n");

  if (argc <= 1) {
    printf("Usage: %s <file.cab>\n", argv[0]);
    return 1;
  }

  for (i = 1; i < argc; i++) {
    if ((fh = fopen((filename = argv[i]), "rb"))) {
      search();
      fclose(fh);
    }
    else {
      perror(filename);
    }
  }
  return 0;
}

#define SEARCH_SIZE (32*1024)
unsigned char search_buf[SEARCH_SIZE];

void search() {
  unsigned char *pstart = &search_buf[0], *pend, *p;
  FILELEN offset, caboff, cablen, foffset, length;
  unsigned long cablen32 = 0, foffset32 = 0;
  int state = 0;

  if (FSEEK(fh, 0, SEEK_END) != 0) {
    perror(filename);
    return;
  }
  filelen = FTELL(fh);
  if (FSEEK(fh, 0, SEEK_SET) != 0) {
    perror(filename);
    return;
  }

  printf("Examining file \"%s\" (%"LD" bytes)...\n", filename, filelen);

  for (offset = 0; offset < filelen; offset += length) {
    /* search length is either the full length of the search buffer,
     * or the amount of data remaining to the end of the file,
     * whichever is less.
     */
    length = filelen - offset;
    if (length > SEARCH_SIZE) length = SEARCH_SIZE;

    /* fill the search buffer with data from disk */
    SEEK(offset);
    READ(&search_buf[0], length);
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
      case 8:  cablen32  = *p++;       state++; break;
      case 9:  cablen32 |= *p++ << 8;  state++; break;
      case 10: cablen32 |= *p++ << 16; state++; break;
      case 11: cablen32 |= *p++ << 24; state++; break;
	
	/* we don't care about bytes 12-15 */
	/* bytes 16-19 are the offset within the cabinet of the filedata */
      case 16: foffset32  = *p++;       state++; break;
      case 17: foffset32 |= *p++ << 8;  state++; break;
      case 18: foffset32 |= *p++ << 16; state++; break;
      case 19: foffset32 |= *p++ << 24;
	/* now we have recieved 20 bytes of potential cab header. */
	/* work out the offset in the file of this potential cabinet */
	caboff = offset + (p-pstart) - 20;
	/* check that the files offset is less than the alleged length
	 * of the cabinet, and that the offset + the alleged length are
	 * 'roughly' within the end of overall file length
	 */
	foffset = (FILELEN) foffset32;
	cablen  = (FILELEN) cablen32;
	if ((foffset < cablen) &&
	    ((caboff + foffset) < (filelen + 32)) &&
	    ((caboff + cablen) < (filelen + 32)) )
	{
	  /* found a potential result - try loading it */
	  printf("Found cabinet header at offset %"LD"\n", caboff);
	  SEEK(caboff);
	  getinfo();
	  offset = caboff + cablen;
	  length = 0;
	  p = pend;
	}
	state = 0;
	break;

      default:
	p++, state++; break;
      } /* switch state */
    } /* while p < pend */
  } /* while offset < filelen */
}

#define CAB_NAMEMAX (1024)

void getinfo() {
  unsigned char buf[64];
  unsigned char namebuf[CAB_NAMEMAX];
  char *name;

  FILELEN offset, base_offset, files_offset, base;
  int num_folders, num_files, num_blocks = 0;
  int header_res = 0, folder_res = 0, data_res = 0;
  int i, x, y;

  base_offset = GETOFFSET;

  READ(&buf, cfhead_SIZEOF);

  x = GETWORD(cfhead_Flags);

  printf(
    "\n*** HEADER SECTION ***\n\n"
    "Cabinet signature      = '%4.4s'\n"
    "Cabinet size           = %u bytes\n"
    "Offset of files        = %"LD"\n"
    "Cabinet format version = %d.%d\n"
    "Number of folders      = %u\n"
    "Number of files        = %u\n"
    "Header flags           = 0x%04x%s%s%s\n"
    "Set ID                 = %u\n"
    "Cabinet set index      = %u\n",

    buf,
    GETLONG(cfhead_CabinetSize),
    files_offset = (GETLONG(cfhead_FileOffset) + base_offset),
    GETBYTE(cfhead_MajorVersion),
    GETBYTE(cfhead_MinorVersion),
    num_folders = GETWORD(cfhead_NumFolders),
    num_files = GETWORD(cfhead_NumFiles),
    x,
    ((x & cfheadPREV_CABINET)    ? " PREV_CABINET"    : ""),
    ((x & cfheadNEXT_CABINET)    ? " NEXT_CABINET"    : ""),
    ((x & cfheadRESERVE_PRESENT) ? " RESERVE_PRESENT" : ""),
    GETWORD(cfhead_SetID),
    GETWORD(cfhead_CabinetIndex)
  );

  if (num_folders == 0) { printf("ERROR: no folders\n"); return; }
  if (num_files == 0) { printf("ERROR: no files\n"); return; }

  if (buf[0]!='M' || buf[1]!='S' || buf[2]!='C' || buf[3]!='F')
    printf("WARNING: cabinet doesn't start with MSCF signature\n");

  if (GETBYTE(cfhead_MajorVersion) > 1
  || GETBYTE(cfhead_MinorVersion) > 3)
    printf("WARNING: format version > 1.3\n");



  if (x & cfheadRESERVE_PRESENT) {
    READ(&buf, cfheadext_SIZEOF);
    header_res = GETWORD(cfheadext_HeaderReserved);
    folder_res = GETBYTE(cfheadext_FolderReserved);
    data_res   = GETBYTE(cfheadext_DataReserved);
  }

  printf("Reserved header space  = %u\n", header_res);
  printf("Reserved folder space  = %u\n", folder_res);
  printf("Reserved datablk space = %u\n", data_res);

  if (header_res > 60000)
    printf("WARNING: header reserved space > 60000\n");

  if (header_res) {
    printf("[Reserved header: offset %"LD", size %u]\n", GETOFFSET,
	   header_res);
    SKIP(header_res);
  }

  if (x & cfheadPREV_CABINET) {
    base = GETOFFSET;
    READ(&namebuf, CAB_NAMEMAX);
    SEEK(base + strlen((char *) namebuf) + 1);
    printf("Previous cabinet file  = %s\n", namebuf);
    if (strlen((char *) namebuf) > 256) printf("WARNING: name length > 256\n");

    base = GETOFFSET;
    READ(&namebuf, CAB_NAMEMAX);
    SEEK(base + strlen((char *) namebuf) + 1);
    printf("Previous disk name     = %s\n", namebuf);
    if (strlen((char *) namebuf) > 256) printf("WARNING: name length > 256\n");
  }

  if (x & cfheadNEXT_CABINET) {
    base = GETOFFSET;
    READ(&namebuf, CAB_NAMEMAX);
    SEEK(base + strlen((char *) namebuf) + 1);
    printf("Next cabinet file      = %s\n", namebuf);
    if (strlen((char *) namebuf) > 256) printf("WARNING: name length > 256\n");

    base = GETOFFSET;
    READ(&namebuf, CAB_NAMEMAX);
    SEEK(base + strlen((char *) namebuf) + 1);
    printf("Next disk name         = %s\n", namebuf);
    if (strlen((char *) namebuf) > 256) printf("WARNING: name length > 256\n");
  }

  printf("\n*** FOLDERS SECTION ***\n");

  for (i = 0; i < num_folders; i++) {
    offset = GETOFFSET;
    READ(&buf, cffold_SIZEOF);

    switch(GETWORD(cffold_CompType) & cffoldCOMPTYPE_MASK) {
    case cffoldCOMPTYPE_NONE:    name = "stored";  break;
    case cffoldCOMPTYPE_MSZIP:   name = "MSZIP";   break;
    case cffoldCOMPTYPE_QUANTUM: name = "Quantum"; break;
    case cffoldCOMPTYPE_LZX:     name = "LZX";     break;
    default:                     name = "unknown"; break;
    }

    printf(
      "\n[New folder at offset %"LD"]\n"
      "Offset of folder       = %"LD"\n"
      "Num. blocks in folder  = %u\n"
      "Compression type       = 0x%04x [%s]\n",

      offset,
      base_offset + GETLONG(cffold_DataOffset),
      GETWORD(cffold_NumBlocks),
      GETWORD(cffold_CompType),
      name
    );

    num_blocks += GETWORD(cffold_NumBlocks);

    if (folder_res) {
      printf("[Reserved folder: offset %"LD", size %u]\n", GETOFFSET,
	     folder_res);
      SKIP(folder_res);
    }
  }

  printf("\n*** FILES SECTION ***\n");

  if (GETOFFSET != files_offset) {
    printf("WARNING: weird file offset in header\n");
    SEEK(files_offset);
  }


  for (i = 0; i < num_files; i++) {
    offset = GETOFFSET;
    READ(&buf, cffile_SIZEOF);

    switch (GETWORD(cffile_FolderIndex)) {
    case cffileCONTINUED_PREV_AND_NEXT:
      name = "continued from previous and to next cabinet";
      break;
    case cffileCONTINUED_FROM_PREV:
      name = "continued from previous cabinet";
      break;
    case cffileCONTINUED_TO_NEXT:
      name = "continued to next cabinet";
      break;
    default:
      name = "normal folder";
      break;
    }
    
    x = GETWORD(cffile_Attribs);
    
    base = GETOFFSET;
    READ(&namebuf, CAB_NAMEMAX);
    SEEK(base + strlen((char *) namebuf) + 1);
    if (strlen((char *) namebuf) > 256) printf("WARNING: name length > 256\n");

    printf(
      "\n[New file at offset %"LD"]\n"
      "File name              = %s%s\n"
      "File size              = %u bytes\n"
      "Offset within folder   = %u\n"
      "Folder index           = 0x%04x [%s]\n"
      "Date / time            = %02d/%02d/%4d %02d:%02d:%02d\n"
      "File attributes        = 0x%02x %s%s%s%s%s%s\n",
      offset,
      x & MSCAB_ATTRIB_UTF_NAME ? "UTF: " : "",
      namebuf,
      GETLONG(cffile_UncompressedSize),
      GETLONG(cffile_FolderOffset),
      GETWORD(cffile_FolderIndex),
      name,
       GETWORD(cffile_Date)       & 0x1f,
      (GETWORD(cffile_Date) >> 5) & 0xf,
      (GETWORD(cffile_Date) >> 9) + 1980,
       GETWORD(cffile_Time) >> 11,
      (GETWORD(cffile_Time) >> 5) & 0x3f,
      (GETWORD(cffile_Time) << 1) & 0x3e,
      x,
      (x & MSCAB_ATTRIB_RDONLY)   ? "RDONLY " : "",
      (x & MSCAB_ATTRIB_HIDDEN)   ? "HIDDEN " : "",
      (x & MSCAB_ATTRIB_SYSTEM)   ? "SYSTEM " : "",
      (x & MSCAB_ATTRIB_ARCH)     ? "ARCH "   : "",
      (x & MSCAB_ATTRIB_EXEC)     ? "EXEC "   : "",
      (x & MSCAB_ATTRIB_UTF_NAME) ? "UTF-8"   : ""
    );
  }

  printf("\n*** DATABLOCKS SECTION ***\n");
  printf("*** Note: offset is BLOCK offset. Add 8 for DATA offset! ***\n\n");

  for (i = 0; i < num_blocks; i++) {
    offset = GETOFFSET;
    READ(&buf, cfdata_SIZEOF);
    x = GETWORD(cfdata_CompressedSize);
    y = GETWORD(cfdata_UncompressedSize);
    printf("Block %6d: offset %12"LD" / csum %08x / c=%5u / u=%5u%s\n",
	   i, offset, GETLONG(cfdata_CheckSum), x, y,
	   ((x > (32768+6144)) || (y > 32768)) ? " INVALID" : "");
    SKIP(x);
  }

}
