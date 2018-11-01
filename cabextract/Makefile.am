AUTOMAKE_OPTIONS =	no-dependencies subdir-objects

bundled_mspack =	mspack/mspack.h \
			mspack/system.h mspack/system.c \
			mspack/cab.h mspack/cabd.c \
			mspack/lzx.h mspack/lzxd.c \
			mspack/mszip.h mspack/mszipd.c \
			mspack/qtm.h mspack/qtmd.c \
			mspack/readbits.h mspack/readhuff.h

EXTRA_DIST =		cabextract.spec \
			doc/cabextract.1 doc/ja/cabextract.1 \
			doc/magic doc/wince_cab_format.html \
			fnmatch_.h getopt.h src/cabsplit \
			src/wince_info src/wince_rename \
			mspack/ChangeLog $(bundled_mspack)

bin_PROGRAMS =		cabextract
noinst_PROGRAMS =	src/cabinfo
man_MANS =		doc/cabextract.1

cabextract_SOURCES =	src/cabextract.c md5.h md5.c
cabextract_LDADD =	@LIBOBJS@ @LIBICONV@

if !EXTERNAL_LIBMSPACK
cabextract_LDADD +=	libmscab.a
noinst_LIBRARIES =	libmscab.a
libmscab_a_SOURCES =	$(bundled_mspack)
AM_CPPFLAGS =		-I$(srcdir)/mspack -DMSPACK_NO_DEFAULT_SYSTEM
endif

