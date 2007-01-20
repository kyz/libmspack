AUTOMAKE_OPTIONS =	no-dependencies

EXTRA_DIST =		cabextract.spec \
			doc/cabextract.1 doc/ja/cabextract.1 \
			doc/magic doc/wince_cab_format.html \
			fnmatch_.h fnmatch_loop.c getopt.h \
			mspack/ChangeLog src/wince_info src/wince_rename

man_MANS =		doc/cabextract.1

AM_CPPFLAGS = 		-I$(srcdir)/mspack -DMSPACK_NO_DEFAULT_SYSTEM
noinst_LIBRARIES =	libmspack.a
libmspack_a_SOURCES =	mspack/mspack.h \
			mspack/system.h mspack/system.c \
			mspack/cab.h mspack/cabd.c \
			mspack/lzx.h mspack/lzxd.c \
			mspack/mszip.h mspack/mszipd.c \
			mspack/qtm.h mspack/qtmd.c

bin_PROGRAMS =		cabextract
noinst_PROGRAMS =	src/cabinfo
cabextract_SOURCES =	src/cabextract.c md5.h md5.c
cabextract_LDADD =	libmspack.a @LIBOBJS@

