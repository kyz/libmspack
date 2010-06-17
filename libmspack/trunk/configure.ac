# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)
AC_INIT([libmspack],[0.0.20100617alpha],[kyzer@4u.net])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([mspack/mspack.h])
AC_CONFIG_HEADER([config.h])

# --enable-debug option
AC_ARG_ENABLE(debug,
  AS_HELP_STRING(--enable-debug,enable debugging),
  enable_debug=$enableval,
  enable_debug=no)
if test x$enable_debug = xyes; then
  AC_DEFINE(DEBUG, 1, [Turn debugging mode on?])
fi
AM_CONDITIONAL(DEBUG, test x$enable_debug = 'xyes')

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_EXEEXT

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([limits.h stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

# largefile support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

# Checks for library functions.
AC_CONFIG_FILES([Makefile doc/Makefile doc/Doxyfile libmspack.pc])
AC_OUTPUT
