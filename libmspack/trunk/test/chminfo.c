#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mspack.h>

#define FILENAME ".chminfo-temp"

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#endif
#if (_FILE_OFFSET_BITS < 64)
# define LU "lu"
# define LD "ld"
#else
# define LU "llu"
# define LD "lld"
#endif

/* endian-neutral reading of little-endian data */
#define __egi32(a,n) ( (((a)[n+3]) << 24) | (((a)[n+2]) << 16) | \
		       (((a)[n+1]) <<  8) |  ((a)[n+0])        )
#define EndGetI64(a) ((((unsigned long long int) __egi32(a,4)) << 32) | \
		      ((unsigned int) __egi32(a,0)))
#define EndGetI32(a) __egi32(a,0)


static int sortfunc(const void *a, const void *b) {
  off_t diff;
  int secdiff =
    ((* ((struct mschmd_file **) a))->section->id) -
    ((* ((struct mschmd_file **) b))->section->id);

  if (secdiff) return secdiff;
  diff = 
    ((* ((struct mschmd_file **) a))->offset) -
    ((* ((struct mschmd_file **) b))->offset);
  return (diff < 0) ? -1 : ((diff > 0) ? 1 : 0);
}

unsigned char *load_sys_data(struct mschm_decompressor *chmd,
			     struct mschmd_header *chm, char *filename)
{
  struct mschmd_file *file;
  unsigned char *data;
  FILE *fh;

  for (file = chm->sysfiles; file; file = file->next) {
    if (strcmp(file->filename, filename) == 0) break;
  }
  if (!file || file->section->id != 0) return NULL;
  if (chmd->extract(chmd, file, FILENAME)) return NULL;
  if (!(data = malloc((size_t) file->length))) return NULL;
  if ((fh = fopen(FILENAME, "rb"))) {
    fread(data, (size_t) file->length, 1, fh);
    fclose(fh);
  }
  else {
    free(data);
    data = NULL;
  }
  unlink(FILENAME);
  return data;
}

int main(int argc, char *argv[]) {
  struct mschm_decompressor *chmd;
  struct mschmd_header *chm;
  struct mschmd_file *file, **f;
  unsigned int numf, i;
  unsigned char *data;

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  MSPACK_SYS_SELFTEST(i);
  if (i) return 0;

  if ((chmd = mspack_create_chm_decompressor(NULL))) {
    for (argv++; *argv; argv++) {
      printf("%s\n", *argv);
      if ((chm = chmd->open(chmd, *argv))) {
	printf("  chmhead_Version     %u\n",      chm->version);
	printf("  chmhead_Timestamp   %u\n",      chm->timestamp);
	printf("  chmhead_LanguageID  %u\n", 	 chm->language);
	printf("  chmhs0_FileLen      %" LU "\n", chm->length);
	printf("  chmhst_OffsetHS1    %" LU "\n", chm->dir_offset);
	printf("  chmhst3_OffsetCS0   %" LU "\n", chm->sec0.offset);
	printf("  chmhs1_NumChunks    %u\n",      chm->num_chunks);
	printf("  chmhs1_ChunkSize    %u\n", chm->chunk_size);
	printf("  chmhs1_Density      %u\n", chm->density);
	printf("  chmhs1_Depth        %u\n", chm->depth);
	printf("  chmhs1_IndexRoot    %d\n", chm->index_root);

	if ((data = load_sys_data(chmd, chm,
	     "::DataSpace/Storage/MSCompressed/ControlData")))
        {
	  printf("  lzxcd_Length        %u\n",    EndGetI32(&data[0]));
	  printf("  lzxcd_Signature     %4.4s\n", &data[4]);
	  printf("  lzxcd_Version       %u\n",    EndGetI32(&data[8]));
	  printf("  lzxcd_ResetInterval %u\n",    EndGetI32(&data[12]));
	  printf("  lzxcd_WindowSize    %u\n",    EndGetI32(&data[16]));
	  printf("  lzxcd_CacheSize     %u\n",    EndGetI32(&data[20]));
	  printf("  lzxcd_Unknown1      %u\n",    EndGetI32(&data[24]));
	  free(data);
	}

	if ((data = load_sys_data(chmd, chm,
	     "::DataSpace/Storage/MSCompressed/Transform/{7FC28940-"
	     "9D31-11D0-9B27-00A0C91E9C7C}/InstanceData/ResetTable")))
        {
	  off_t contents = chm->sec0.offset;
	  printf("  lzxrt_Unknown1      %u\n",      EndGetI32(&data[0]));
	  printf("  lzxrt_NumEntries    %u\n",      EndGetI32(&data[4]));
	  printf("  lzxrt_EntrySize     %u\n",      EndGetI32(&data[8]));
	  printf("  lzxrt_TableOffset   %u\n",      EndGetI32(&data[12]));
	  printf("  lzxrt_UncompLen     %" LU "\n", EndGetI64(&data[16]));
	  printf("  lzxrt_CompLen       %" LU "\n", EndGetI64(&data[24]));
	  printf("  lzxrt_FrameLen      %u\n",      EndGetI32(&data[32]));

	  for (file = chm->sysfiles; file; file = file->next) {
	    if (strcmp(file->filename,
		       "::DataSpace/Storage/MSCompressed/Content") == 0)
	    {
	      contents += file->offset;
	      break;
	    }
	  }

	  printf("  - reset table (uncomp offset -> stream offset "
		 "[real offset, length in file]\n");

	  numf = EndGetI32(&data[4]);
	  switch (EndGetI32(&data[8])) {
	  case 4:
	    for (i = 0; i < numf; i++) {
	      unsigned int tablepos = EndGetI32(&data[12]) + (i * 4);
	      unsigned int rtdata = EndGetI32(&data[tablepos]);
	      printf("    %-10u -> %-10u [ %u %u ]\n",
		     i * EndGetI32(&data[32]), rtdata, contents + rtdata,
		     (i == (numf-1))
		     ? (EndGetI32(&data[24]) - rtdata)
		     : (EndGetI32(&data[tablepos + 4]) - rtdata)
		     );
	    }
	    break;
	  case 8:
	    for (i = 0; i < numf; i++) {
	      unsigned int tablepos = EndGetI32(&data[12]) + (i * 8);
	      unsigned long long int rtdata = EndGetI64(&data[tablepos]);
	      printf("    %-10llu -> %-10llu [ %llu %llu ]\n",
		     i * EndGetI64(&data[32]), rtdata, contents + rtdata,
		     (i == (numf-1))
		     ? (EndGetI64(&data[24]) - rtdata)
		     : (EndGetI64(&data[tablepos + 8]) - rtdata)
		     );
	    }
	    break;
	  }
	  free(data);
	}

	printf("  - system files (offset, length, filename):\n");
	for (file = chm->sysfiles; file; file = file->next) {
	  printf("    %u:%-10" LU " %-10" LU " %s\n", file->section->id,
		 file->offset, file->length, file->filename);
	}

	printf("  - files (offset, length, filename):\n");
	for (numf=0, file=chm->files; file; file = file->next) numf++;
	if ((f = calloc(numf, sizeof(struct mschmd_file *)))) {
	  for (i=0, file=chm->files; file; file = file->next) f[i++] = file;
	  qsort(f, numf, sizeof(struct mschmd_file *), &sortfunc);
	  for (i = 0; i < numf; i++) {
	    printf("    %u:%-10" LU " %-10" LU " %s\n", f[i]->section->id,
		   f[i]->offset, f[i]->length, f[i]->filename);
	  }
	  free(f);
	}
	chmd->close(chmd, chm);
      }
    }
    mspack_destroy_chm_decompressor(chmd);
  }
  return 0;
}
