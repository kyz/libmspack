EXTRA_DIST =		debian
INCLUDES = 		-I$(srcdir)/mspack
CFLAGS =		-DDEBUG -g -Wall -Wsign-compare -Wconversion -pedantic \
			$(INCLUDES)

noinst_LTLIBRARIES =	libmspack.la libmscabd.la libmschmd.la
noinst_PROGRAMS =	test/cabd_md5 test/cabd_test test/cabextract_md5 \
			test/cabrip test/chmd_md5 test/chmx test/chminfo

libmspack_la_SOURCES =	mspack/mspack.h \
			mspack/system.h mspack/system.c \
			mspack/cab.h mspack/cabc.c mspack/cabd.c \
			mspack/chm.h mspack/chmc.c mspack/chmd.c \
			mspack/hlp.h mspack/hlpc.c mspack/hlpd.c \
			mspack/lit.h mspack/litc.c mspack/litd.c \
			mspack/kwaj.h mspack/kwajc.c mspack/kwajd.c \
			mspack/szdd.h mspack/szddc.c mspack/szddd.c \
			mspack/lzx.h mspack/lzxc.c mspack/lzxd.c \
			mspack/mszip.h mspack/mszipc.c mspack/mszipd.c \
			mspack/qtm.h mspack/qtmd.c
libmspack_la_LDFLAGS =	-export-symbols-regex '^mspack_'

libmscabd_la_SOURCES =	mspack/mspack.h \
			mspack/system.h mspack/system.c \
			mspack/cab.h mspack/cabd.c \
			mspack/lzx.h mspack/lzxd.c \
			mspack/mszip.h mspack/mszipd.c \
			mspack/qtm.h mspack/qtmd.c
libmscabd_la_LDFLAGS =	-export-symbols-regex '^mspack_'

libmschmd_la_SOURCES =	mspack/mspack.h \
			mspack/system.h mspack/system.c \
			mspack/chm.h mspack/chmd.c \
			mspack/lzx.h mspack/lzxd.c
libmschmd_la_LDFLAGS =	-export-symbols-regex '^mspack_'

test_chmx_SOURCES =	test/chmx.c libmschmd.la
test_chmx_LDADD =	libmschmd.la

test_chmd_md5_SOURCES =	test/chmd_md5.c test/md5.c libmschmd.la
test_chmd_md5_LDADD =	libmschmd.la

test_cabd_md5_SOURCES =	test/cabd_md5.c test/md5.c libmspack.la
test_cabd_md5_LDADD =	libmspack.la

test_cabextract_md5_SOURCES = test/cabextract_md5.c test/md5.c

test_cabd_test_SOURCES=	test/cabd_test.c libmspack.la
test_cabd_test_LDADD = 	libmspack.la

test_cabrip_SOURCES =	test/cabrip.c libmspack.la
test_cabrip_LDADD =	libmspack.la

test_chminfo_SOURCES = test/chminfo.c libmschmd.la
test_chminfo_LDADD   = libmschmd.la
