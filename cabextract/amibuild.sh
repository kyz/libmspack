#!/bin/sh
# see https://github.com/cahirwpz/amigaos-cross-toolchain

cat >config.h <<EOF
#define HAVE_CTYPE_H 1
#define HAVE_DIRENT_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FNMATCH_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_MKDIR 1
#define HAVE_STDARG_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCASECMP 1
#define HAVE_STRCHR 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UTIME 1
#define HAVE_UTIMES 1
#define HAVE_UTIME_H 1
#define ICONV_CONST const
#define LATIN1_FILENAMES 1
#define STDC_HEADERS 1
#define TIME_WITH_SYS_TIME 1
#define VERSION "1.8"
#define WORDS_BIGENDIAN 1
EOF
cp fnmatch_.h fnmatch.h

PATH=/usr/local/amiga/bin:$PATH
CFLAGS='-Wall -O2 -s -DHAVE_CONFIG_H -I. -Imspack'
SRCS="mspack/*.c src/cabextract.c md5.c fnmatch.c"

rm -f *.lha

ppc-amigaos-gcc $CFLAGS -DHAVE_FSEEKO -DHAVE_ICONV -DHAVE_UMASK $SRCS -o cabextract &&
ppc-amigaos-gcc $CFLAGS -DHAVE_FSEEKO src/cabinfo.c -o cabinfo &&
lha a cabextract_OS4.lha cabextract cabinfo

m68k-amigaos-gcc $CFLAGS -DNDEBUG -noixemul $SRCS getopt.c getopt1.c -o cabextract &&
m68k-amigaos-gcc $CFLAGS -DNDEBUG -noixemul src/cabinfo.c -o cabinfo &&
lha a cabextract.lha cabextract cabinfo
