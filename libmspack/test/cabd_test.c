/* cabinet decompression regression test suite */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mspack.h>
#include <system.h>

#define __tf3(x) #x
#define __tf2(x) __tf3(x)
#define TESTFILE(fname) (__tf2(TEST_FILES) "/" fname)

unsigned int test_count = 0;

#define TEST(x) do {\
  test_count++; \
  if (!(x)) {printf("%s:%d FAILED %s\n",__func__,__LINE__,#x);exit(1);} \
} while (0)

/* open where cab file doesn't exist */
void cabd_open_test_01() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab;

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  cab = cabd->open(cabd, "!!!FILE_WHICH_DOES_NOT_EXIST");
  TEST(cab == NULL);
  TEST(cabd->last_error(cabd) == MSPACK_ERR_OPEN);

  mspack_destroy_cab_decompressor(cabd);
}

/* normal cab file with 2 files and one folder.
 * check ALL headers are read correctly */
void cabd_open_test_02() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab;
  struct mscabd_folder *folder;
  struct mscabd_file *file;

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  cab = cabd->open(cabd, TESTFILE("normal_2files_1folder.cab"));
  TEST(cab != NULL);

  TEST(cab->next == NULL);
  TEST(cab->base_offset == 0);
  TEST(cab->length == 253);
  TEST(cab->prevcab == NULL); TEST(cab->nextcab == NULL);
  TEST(cab->prevname == NULL); TEST(cab->nextname == NULL);
  TEST(cab->previnfo == NULL); TEST(cab->nextinfo == NULL);
  TEST(cab->set_id = 1570); TEST(cab->set_index == 0);
  TEST(cab->header_resv == 0);
  TEST(cab->flags == 0);

  folder = cab->folders;
  TEST(folder != NULL);
  TEST(folder->next == NULL);
  TEST(folder->comp_type == 0);
  TEST(folder->num_blocks == 1);

  file = cab->files;
  TEST(file != NULL);
  TEST(strcmp(file->filename, "hello.c") == 0);
  TEST(file->length == 77);
  TEST(file->attribs == 0x20);
  TEST(file->time_h == 11);TEST(file->time_m == 13);TEST(file->time_s == 52);
  TEST(file->date_d == 12);TEST(file->date_m == 3);TEST(file->date_y == 1997);
  TEST(file->folder == folder); TEST(file->offset == 0);

  file = file->next;
  TEST(file != NULL);
  TEST(strcmp(file->filename, "welcome.c") == 0);
  TEST(file->length == 74);
  TEST(file->attribs == 0x20);
  TEST(file->time_h == 11);TEST(file->time_m == 15);TEST(file->time_s == 14);
  TEST(file->date_d == 12);TEST(file->date_m == 3);TEST(file->date_y == 1997);
  TEST(file->folder == folder); TEST(file->offset == 77);

  cabd->close(cabd, cab);
  mspack_destroy_cab_decompressor(cabd);
}

/* cabs with reserve headers set, ensure they all load correctly */
void cabd_open_test_03() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab;
  unsigned int i;
  const char *files[] = {
    TESTFILE("reserve_---.cab"),
    TESTFILE("reserve_--D.cab"),
    TESTFILE("reserve_-F-.cab"),
    TESTFILE("reserve_-FD.cab"),
    TESTFILE("reserve_H--.cab"),
    TESTFILE("reserve_H-D.cab"),
    TESTFILE("reserve_HF-.cab"),
    TESTFILE("reserve_HFD.cab"),
  };

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  for (i = 0; i < (sizeof(files)/sizeof(char *)); i++) {
    cab = cabd->open(cabd, files[i]);
    TEST(cab != NULL);
    TEST(cab->files != NULL);
    TEST(cab->files->next != NULL);
    TEST(strcmp(cab->files->filename, "test1.txt") == 0);
    TEST(strcmp(cab->files->next->filename, "test2.txt") == 0);
    cabd->close(cabd, cab);
  }

  mspack_destroy_cab_decompressor(cabd);
}

/* some bad cabs, should not load */
void cabd_open_test_04() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab;

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  /* cab has enough data for a header, but does not contain real cab data
   * result should be MSPACK_ERR_SIGNATURE */
  cab = cabd->open(cabd, TESTFILE("bad_signature.cab"));
  TEST(cab == NULL);

  /* cab has 0 folders */
  cab = cabd->open(cabd, TESTFILE("bad_nofolders.cab"));
  TEST(cab == NULL);

  /* cab has 0 files */
  cab = cabd->open(cabd, TESTFILE("bad_nofiles.cab"));
  TEST(cab == NULL);

  /* second file in the cab has a folder index for a non-existant folder */
  cab = cabd->open(cabd, TESTFILE("bad_folderindex.cab"));
  TEST(cab == NULL);

  /* cab has one file with empty filename */
  cab = cabd->open(cabd, TESTFILE("filename-read-violation-1.cab"));
  TEST(cab == NULL);

  mspack_destroy_cab_decompressor(cabd);
}

/* cabs which have been cut short
 * result should be MSPACK_ERR_READ for missing headers or
 * MSPACK_ERR_DATAFORMAT for missing/partial strings.
 * If only data blocks are missing, the cab should open()
 */
void cabd_open_test_05() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab;
  unsigned int i;
  const char *files[] = {
    TESTFILE("partial_shortheader.cab"),
    TESTFILE("partial_shortextheader.cab"),
    TESTFILE("partial_nofolder.cab"),
    TESTFILE("partial_shortfolder.cab"),
    TESTFILE("partial_nofiles.cab"),
    TESTFILE("partial_shortfile1.cab"),
    TESTFILE("partial_shortfile2.cab"),
  };
  const char *str_files[] = {
    TESTFILE("partial_str_nopname.cab"),
    TESTFILE("partial_str_shortpname.cab"),
    TESTFILE("partial_str_nopinfo.cab"),
    TESTFILE("partial_str_shortpinfo.cab"),
    TESTFILE("partial_str_nonname.cab"),
    TESTFILE("partial_str_shortnname.cab"),
    TESTFILE("partial_str_noninfo.cab"),
    TESTFILE("partial_str_shortninfo.cab"),
    TESTFILE("partial_str_nofname.cab"),
    TESTFILE("partial_str_shortfname.cab"),
  };

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  for (i = 0; i < (sizeof(files)/sizeof(char *)); i++) {
    cab = cabd->open(cabd, files[i]);
    TEST(cab == NULL);
    TEST(cabd->last_error(cabd) == MSPACK_ERR_READ);
  }

  for (i = 0; i < (sizeof(str_files)/sizeof(char *)); i++) {
    cab = cabd->open(cabd, str_files[i]);
    TEST(cab == NULL);
    TEST(cabd->last_error(cabd) == MSPACK_ERR_DATAFORMAT ||
         cabd->last_error(cabd) == MSPACK_ERR_READ);
  }

  /* lack of data blocks should NOT be a problem for merely reading */
  cab = cabd->open(cabd, TESTFILE("partial_nodata.cab"));
  TEST(cab != NULL);

  cabd->close(cabd, cab);
  mspack_destroy_cab_decompressor(cabd);
}

/* open cab with 255 character filename (maximum allowed) */
void cabd_open_test_06() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab;

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  cab = cabd->open(cabd, TESTFILE("normal_255c_filename.cab"));
  TEST(cab != NULL);

  cabd->close(cabd, cab);
  mspack_destroy_cab_decompressor(cabd);
}


/* open where search file doesn't exist */
void cabd_search_test_01() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab;

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  cab = cabd->search(cabd, "!!!FILE_WHICH_DOES_NOT_EXIST");
  TEST(cab == NULL);
  TEST(cabd->last_error(cabd) == MSPACK_ERR_OPEN);

  mspack_destroy_cab_decompressor(cabd);
}
  
/* search file using 1-byte buffer */
void cabd_search_test_02() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab;

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  cabd->set_param(cabd, MSCABD_PARAM_SEARCHBUF, 1);
  cab = cabd->search(cabd, TESTFILE("search_basic.cab"));
  cabd->set_param(cabd, MSCABD_PARAM_SEARCHBUF, 32768);

  TEST(cab != NULL);
  TEST(cab->files != NULL);
  TEST(cab->base_offset == 6);
  TEST(cab->files->next != NULL);
  TEST(strcmp(cab->files->filename, "hello.c") == 0);
  TEST(strcmp(cab->files->next->filename, "welcome.c") == 0);

  TEST(cab->next != NULL);
  TEST(cab->next->base_offset == 265);
  TEST(cab->next->files != NULL);
  TEST(cab->next->files->next != NULL);
  TEST(strcmp(cab->next->files->filename, "hello.c") == 0);
  TEST(strcmp(cab->next->files->next->filename, "welcome.c") == 0);

  TEST(cab->next->next == NULL);

  cabd->close(cabd, cab);
  mspack_destroy_cab_decompressor(cabd);
}

/* tricky searches */
void cabd_search_test_03() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab;

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  /* there is only ONE cab in this file. it is prepended by 4 bytes, "MSCF"
   * (heh) and reserved fields in the real cab are filled in so the fake one
   * looks real to the scanner but not the real reader
   */
  cab = cabd->search(cabd, TESTFILE("search_tricky1.cab"));
  TEST(cab != NULL);
  TEST(cab->next == NULL);
  TEST(cab->files != NULL);
  TEST(cab->base_offset == 4);
  TEST(cab->files->next != NULL);
  TEST(strcmp(cab->files->filename, "hello.c") == 0);
  TEST(strcmp(cab->files->next->filename, "welcome.c") == 0);

  cabd->close(cabd, cab);
  mspack_destroy_cab_decompressor(cabd);
}

/* basic parameter failures */
void cabd_merge_test_01() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab1, *cab2;

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  cab1 = cabd->open(cabd, TESTFILE("multi_basic_pt1.cab"));
  cab2 = cabd->open(cabd, TESTFILE("multi_basic_pt2.cab"));
  TEST(cab1 != NULL);
  TEST(cab2 != NULL);
  TEST(cabd->append(cabd,  cab1, NULL) != MSPACK_ERR_OK);
  TEST(cabd->append(cabd,  NULL, cab1) != MSPACK_ERR_OK);
  TEST(cabd->append(cabd,  cab1, cab1) != MSPACK_ERR_OK);
  TEST(cabd->prepend(cabd, cab1, NULL) != MSPACK_ERR_OK);
  TEST(cabd->prepend(cabd, NULL, cab1) != MSPACK_ERR_OK);
  TEST(cabd->prepend(cabd, cab1, cab1) != MSPACK_ERR_OK);

  /* merge cabs, then try merging again every other way */
  TEST(cabd->append(cabd,  cab1, cab2) == MSPACK_ERR_OK);
  TEST(cabd->append(cabd,  cab2, cab1) != MSPACK_ERR_OK);
  TEST(cabd->prepend(cabd, cab1, cab2) != MSPACK_ERR_OK);
  TEST(cabd->prepend(cabd, cab2, cab1) != MSPACK_ERR_OK);
  TEST(cabd->append(cabd,  cab1, cab2) != MSPACK_ERR_OK);

  cabd->close(cabd, cab1);
  mspack_destroy_cab_decompressor(cabd);
}
  
/* test merging a normal 5 part single folder cabinet set with slightly
 * haphazard ordering.  should still merge fine */
void cabd_merge_test_02() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab[5];

  cabd = mspack_create_cab_decompressor(NULL);
  TEST(cabd != NULL);

  cab[0] = cabd->open(cabd, TESTFILE("multi_basic_pt1.cab"));
  cab[1] = cabd->open(cabd, TESTFILE("multi_basic_pt2.cab"));
  cab[2] = cabd->open(cabd, TESTFILE("multi_basic_pt3.cab"));
  cab[3] = cabd->open(cabd, TESTFILE("multi_basic_pt4.cab"));
  cab[4] = cabd->open(cabd, TESTFILE("multi_basic_pt5.cab"));
  TEST(cab[0] != NULL);
  TEST(cab[1] != NULL);
  TEST(cab[2] != NULL);
  TEST(cab[3] != NULL);
  TEST(cab[4] != NULL);
  TEST(cabd->append(cabd,  cab[0], cab[1]) == MSPACK_ERR_OK);
  TEST(cabd->prepend(cabd, cab[2], cab[1]) == MSPACK_ERR_OK);
  TEST(cabd->append(cabd,  cab[3], cab[4]) == MSPACK_ERR_OK);
  TEST(cabd->prepend(cabd, cab[3], cab[2]) == MSPACK_ERR_OK);

  TEST(cab[0]->files != NULL);
  TEST(cab[0]->files->next != NULL);
  TEST(cab[0]->files->next->next != NULL);
  TEST(cab[0]->files->next->next->next == NULL);
  TEST(cab[0]->files == cab[1]->files);
  TEST(cab[1]->files == cab[2]->files);
  TEST(cab[2]->files == cab[3]->files);
  TEST(cab[3]->files == cab[4]->files);

  TEST(cab[0]->folders != NULL);
  TEST(cab[0]->folders->next == NULL);
  TEST(cab[0]->folders == cab[1]->folders);
  TEST(cab[1]->folders == cab[2]->folders);
  TEST(cab[2]->folders == cab[3]->folders);
  TEST(cab[3]->folders == cab[4]->folders);

  cabd->close(cabd, cab[0]);
  mspack_destroy_cab_decompressor(cabd);
}

/* test bad cabinets cannot be extracted */
void cabd_extract_test_01() {
    struct mscab_decompressor *cabd;
    struct mscabd_cabinet *cab;
    struct mscabd_file *file;
    unsigned int i;
    const char *files[] = {
        TESTFILE("cve-2010-2800-mszip-infinite-loop.cab"),
        TESTFILE("cve-2014-9556-qtm-infinite-loop.cab"),
        TESTFILE("cve-2015-4470-mszip-over-read.cab"),
        TESTFILE("cve-2015-4471-lzx-under-read.cab"),
        TESTFILE("filename-read-violation-2.cab"),
        TESTFILE("filename-read-violation-3.cab"),
        TESTFILE("filename-read-violation-4.cab"),
        TESTFILE("lzx-main-tree-no-lengths.cab"),
        TESTFILE("lzx-premature-matches.cab"),
        TESTFILE("qtm-max-size-block.cab"),
    };

    cabd = mspack_create_cab_decompressor(NULL);
    TEST(cabd != NULL);

    for (i = 0; i < (sizeof(files)/sizeof(char *)); i++) {
        cab = cabd->open(cabd, files[i]);
        TEST(cab != NULL);
        TEST(cab->files != NULL);
        for (file = cab->files; file; file = file->next) {
            int err = cabd->extract(cabd, file, "/dev/null");
            TEST(err == MSPACK_ERR_DATAFORMAT || err == MSPACK_ERR_DECRUNCH);
        }
        cabd->close(cabd, cab);
    }
    mspack_destroy_cab_decompressor(cabd);
}

/* test that CVE-2014-9732 is fixed */
void cabd_extract_test_02() {
    struct mscab_decompressor *cabd;
    struct mscabd_cabinet *cab;
    int err;

    /* the first file in this cabinet belongs to a valid folder. The
     * second belongs to an invalid folder. Unpacking files 1, 2, 1
     * caused cabd.c to try and free the invalid folder state left by
     * extracting from folder 2, which caused a jump to NULL / segfault
     */
    cabd = mspack_create_cab_decompressor(NULL);
    TEST(cabd != NULL);
    cab = cabd->open(cabd, TESTFILE("cve-2014-9732-folders-segfault.cab"));
    TEST(cab != NULL);
    err = cabd->extract(cabd, cab->files, "/dev/null");
    TEST(err == MSPACK_ERR_OK);
    err = cabd->extract(cabd, cab->files->next, "/dev/null");
    TEST(err == MSPACK_ERR_DATAFORMAT || err == MSPACK_ERR_DECRUNCH);
    err = cabd->extract(cabd, cab->files, "/dev/null");
    TEST(err == MSPACK_ERR_OK);
    cabd->close(cabd, cab);
    mspack_destroy_cab_decompressor(cabd);
}

int main() {
  int selftest;

  MSPACK_SYS_SELFTEST(selftest);
  TEST(selftest == MSPACK_ERR_OK);

  cabd_open_test_01();
  cabd_open_test_02();
  cabd_open_test_03();
  cabd_open_test_04();
  cabd_open_test_05();
  cabd_open_test_06();

  cabd_search_test_01();
  cabd_search_test_02();
  cabd_search_test_03();

  cabd_merge_test_01();
  cabd_merge_test_02();

  cabd_extract_test_01();
  cabd_extract_test_02();

  printf("ALL %d TESTS PASSED.\n", test_count);
  return 0;
}
