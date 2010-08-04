#!/bin/sh
# see http://www.zerohero.se/cross/os3.html
# and http://www.zerohero.se/cross/os4.html

cat >config.h <<EOF
#define HAVE_CTYPE_H 1
#define HAVE_DIRENT_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FNMATCH_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MEMCPY 1
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
#define STDC_HEADERS 1
#define TIME_WITH_SYS_TIME 1
#define VERSION "1.3"
#define WORDS_BIGENDIAN 1
EOF
cp fnmatch_.h fnmatch.h

PATH=/usr/local/amiga/bin:$PATH
export PATH

rm -f *.lha

ppc-amigaos-gcc -Wall -O2 -o cabextract -DHAVE_CONFIG_H -DHAVE_FSEEKO -I. -Imspack mspack/*.c src/cabextract.c md5.c fnmatch.c
lha a cabextract_OS4.lha cabextract

m68k-amigaos-gcc -Wall -O2 -o cabextract -DHAVE_CONFIG_H -DNDEBUG -I. -Imspack mspack/*.c src/cabextract.c md5.c fnmatch.c getopt.c getopt1.c
lha a cabextract.lha cabextract
