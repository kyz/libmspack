# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.57)
AC_INIT(cabextract, 1.0, kyzer@4u.net)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/cabextract.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_EXEEXT

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_TIME
AC_HEADER_DIRENT
AC_CHECK_HEADERS([ctype.h errno.h fnmatch.h libintl.h limits.h stdlib.h \
	string.h strings.h utime.h stdarg.h sys/stat.h sys/time.h sys/types.h \
	getopt.h wchar.h wctype.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_SYS_LARGEFILE

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_FNMATCH
AC_FUNC_FSEEKO
AC_FUNC_MKTIME
AC_CHECK_FUNCS([memcpy strcasecmp strchr utime utimes])
AC_CHECK_FUNCS([getopt_long],,[AC_CHECK_LIB([gnugetopt], [getopt_long],
  [AC_DEFINE([HAVE_GETOPT_LONG])],[AC_LIBOBJ(getopt) AC_LIBOBJ(getopt1)])])
AC_REPLACE_FNMATCH
AC_REPLACE_FUNCS([mempcpy])

AC_CONFIG_FILES([Makefile cabextract.spec])
AC_OUTPUT
