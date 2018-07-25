AUTOMAKE_OPTIONS =	no-dependencies subdir-objects

EXTRA_DIST =		cabextract.spec \
			doc/cabextract.1 doc/ja/cabextract.1 \
			doc/magic doc/wince_cab_format.html \
			fnmatch_.h getopt.h \
			mspack/ChangeLog src/cabsplit \
			src/wince_info src/wince_rename

man_MANS =		doc/cabextract.1

mspack_sources =	mspack/mspack.h \
			mspack/system.h mspack/system.c \
			mspack/cab.h mspack/cabd.c \
			mspack/lzx.h mspack/lzxd.c \
			mspack/mszip.h mspack/mszipd.c \
			mspack/qtm.h mspack/qtmd.c \
			mspack/readbits.h mspack/readhuff.h
if ! EXTERNAL_LIBMSPACK
AM_CPPFLAGS =           -I$(srcdir)/mspack -DMSPACK_NO_DEFAULT_SYSTEM
noinst_LIBRARIES =      libmspack.a
libmspack_a_SOURCES =	$(mspack_sources)
else
EXTRA_DIST +=		$(mspack_sources)
endif

bin_PROGRAMS =		cabextract
noinst_PROGRAMS =	src/cabinfo
cabextract_SOURCES =	src/cabextract.c md5.h md5.c
if ! EXTERNAL_LIBMSPACK
cabextract_LDADD =	libmspack.a @LIBOBJS@ @LIBICONV@
else
cabextract_LDADD =	@LIBOBJS@ @LIBICONV@ $(LIBMSPACK_LIBS)
endif

