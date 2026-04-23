/* This file is part of libmspack.
 * (C) 2021 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <mspack.h>

/* mspack_system implementation which reads memory and writes nothing */
struct mem_buf {
    const uint8_t *data;
    size_t length;
};
struct mem_file {
    char *data;
    size_t length, posn;
};
static struct mspack_file *m_open(struct mspack_system *self, const char *filename, int mode) {
    struct mem_buf *fn = (struct mem_buf *) filename;
    struct mem_file *fh;
    if ((mode != MSPACK_SYS_OPEN_READ && mode != MSPACK_SYS_OPEN_WRITE) ||
        (mode == MSPACK_SYS_OPEN_READ && fn == NULL)) return NULL;

    if ((fh = (struct mem_file *) malloc(sizeof(struct mem_file)))) {
        if (mode == MSPACK_SYS_OPEN_READ) {
            fh->data = (char *) fn->data;
            fh->length = fn->length;
        }
        else {
            fh->data = NULL;
            fh->length = 0;
        }
        fh->posn = 0;
        return (struct mspack_file *) fh;
    }
    return NULL;
}
static void m_close(struct mspack_file *file) {
    free(file);
}
static int m_read(struct mspack_file *file, void *buffer, int bytes) {
    struct mem_file *fh = (struct mem_file *) file;
    int avail;
    if (!fh || !buffer || bytes < 0) return -1;
    avail = fh->length - fh->posn;
    if (bytes > avail) bytes = avail;
    if (bytes > 0) memcpy(buffer, &fh->data[fh->posn], bytes);
    fh->posn += bytes;
    return bytes;
}
static int m_write(struct mspack_file *file, void *buffer, int bytes) {
    return (!file || !buffer || bytes < 0) ? -1 : bytes;
}
static int m_seek(struct mspack_file *file, off_t offset, int mode) {
    struct mem_file *fh = (struct mem_file *) file;
    if (!fh) return 1;
    switch (mode) {
    case MSPACK_SYS_SEEK_START: break;
    case MSPACK_SYS_SEEK_CUR:   offset += (off_t) fh->posn; break;
    case MSPACK_SYS_SEEK_END:   offset += (off_t) fh->length; break;
    default: return 1;
    }
    if ((offset < 0) || (offset > (off_t) fh->length)) return 1;
    fh->posn = (size_t) offset;
    return 0;
}
static off_t m_tell(struct mspack_file *file) {
    struct mem_file *fh = (struct mem_file *) file;
    return (fh) ? (off_t) fh->posn : -1;
}
static void m_msg(struct mspack_file *file, const char *format, ...) {
}
static void *m_alloc(struct mspack_system *self, size_t bytes) {
    return malloc(bytes);
}
static void m_free(void *buffer) {
    free(buffer);
}
static void m_copy(void *src, void *dest, size_t bytes) {
    memcpy(dest, src, bytes);
}
static struct mspack_system mem_system = {
    &m_open, &m_close, &m_read, &m_write, &m_seek, &m_tell,
    &m_msg, &m_alloc, &m_free, &m_copy, NULL
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    struct mscab_decompressor *cabd;
    struct mscabd_cabinet *cab;
    struct mscabd_file *file;
    struct mem_buf source = { data, size };

    if ((cabd = mspack_create_cab_decompressor(&mem_system))) {
        if ((cab = cabd->open(cabd, (char *) &source))) {
            /* attempt to extract all files (to nowhere) */
            for (file = cab->files; file; file = file->next) {
                cabd->extract(cabd, file, NULL);
            }
            cabd->close(cabd, cab);
        }
    }
    mspack_destroy_cab_decompressor(cabd);
    return 0;
}
