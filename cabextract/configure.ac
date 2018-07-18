# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([cabextract],[1.7],[kyzer@cabextract.org.uk])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/cabextract.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_AR
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_EXEEXT

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_TIME
AC_HEADER_DIRENT
AC_CHECK_HEADERS([ctype.h errno.h fnmatch.h libintl.h limits.h locale.h stdlib.h \
	string.h strings.h utime.h stdarg.h sys/stat.h sys/time.h sys/types.h \
	getopt.h wchar.h wctype.h inttypes.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF([off_t])
AC_SYS_LARGEFILE

# Checks for library functions.
AC_FUNC_FNMATCH
AC_FUNC_FSEEKO
AX_FUNC_MKDIR
AC_FUNC_MKTIME
AC_CHECK_FUNCS([memcpy memmove strcasecmp strchr towlower umask utime utimes])
AC_CHECK_FUNCS([getopt_long],,[AC_CHECK_LIB([gnugetopt], [getopt_long],
  [AC_DEFINE([HAVE_GETOPT_LONG])],[AC_LIBOBJ(getopt) AC_LIBOBJ(getopt1)])])
AC_REPLACE_FNMATCH
AM_ICONV

# use an external libmspack if requested
cabextract_external_libmspack=no
AC_ARG_WITH([external-libmspack],
 [AS_HELP_STRING([--with-external-libmspack],[use external libmspack (default no)])],
 [cabextract_external_libmspack=$withval])

if test "z$cabextract_external_libmspack" != 'zno'; then
 AC_CHECK_HEADER([mspack.h], ,[AC_MSG_ERROR([Cannot find libmspack header])])
fi

if test "z$cabextract_external_libmspack" != 'zno'; then
 AC_CHECK_LIB([mspack],[mspack_create_cab_compressor], 
  [LIBMSPACK_LIBS=-lmspack],
  [AC_MSG_ERROR([Cannot find libmspack library])])
fi
AC_SUBST([LIBMSPACK_LIBS])
AM_CONDITIONAL([EXTERNAL_LIBMSPACK], [test "z$cabextract_external_libmspack" != 'zno'])

AC_CONFIG_FILES([Makefile cabextract.spec])
AC_OUTPUT
