/* This file is part of libmspack.
 * (C) 2003-2018 Stuart Caie.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

#ifndef MSPACK_SYSTEM_H
#define MSPACK_SYSTEM_H 1

#ifdef __cplusplus
extern "C" {
#endif

/* ensure config.h is read before mspack.h */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <mspack.h>
#include <macros.h>

/* assume <string.h> exists */
#include <string.h>

/* fix for problem with GCC 4 and glibc (thanks to Ville Skytta)
 * http://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=150429
 */
#ifdef read
# undef read
#endif

/* CAB supports searching through files over 4GB in size, and the CHM file
 * format actively uses 64-bit offsets. These can only be fully supported
 * if the system the code runs on supports large files. If not, the library
 * will work as normal using only 32-bit arithmetic, but if an offset
 * greater than 2GB is detected, an error message indicating the library
 * can't support the file should be printed.
 */
#include <limits.h>
#if ((defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS >= 64) || \
     (defined(FILESIZEBITS)      && FILESIZEBITS      >= 64) || \
     defined(_LARGEFILE_SOURCE) || defined(_LARGEFILE64_SOURCE) || \
     SIZEOF_OFF_T >= 8)
# define LARGEFILE_SUPPORT 1
#else
extern const char *largefile_msg;
#endif

extern struct mspack_system *mspack_default_system;

/* returns the length of a file opened for reading */
extern int mspack_sys_filelen(struct mspack_system *system,
                              struct mspack_file *file, off_t *length);

/* validates a system structure */
extern int mspack_valid_system(struct mspack_system *sys);

#ifdef __cplusplus
}
#endif

#endif
