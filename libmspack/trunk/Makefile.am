# add "-DDEBUG" to enable debug output
# add "-DMSPACK_NO_DEFAULT_SYSTEM" to remove default mspack_system
MSPACK_FLAGS =

EXTRA_DIST =		debian doc test/test_files
dist-hook:
	-rm -rf `find $(distdir) -name CVS`
	-rm -f $(distdir)/*.la

AM_CFLAGS =		-std=c99 -Wall -Wsign-compare -Wconversion -pedantic \
			-I$(srcdir)/mspack $(MSPACK_FLAGS)

lib_LTLIBRARIES =	libmspack.la
include_HEADERS =	mspack/mspack.h

noinst_LTLIBRARIES =	libmscabd.la libmschmd.la
noinst_PROGRAMS =	test/cabd_md5 test/cabd_memory test/cabd_test
			test/cabextract_md5 test/cabrip test/chmd_md5
			test/chmx test/chminfo

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
			mspack/qtm.h mspack/qtmd.c \
			mspack/lzss.h mspack/des.h mspack/sha.h
libmspack_la_LDFLAGS =	-export-symbols-regex '^mspack_' -version-info 1:0:1

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

test_chmd_md5_SOURCES =	test/chmd_md5.c test/md5.c test/md5.h libmschmd.la
test_chmd_md5_LDADD =	libmschmd.la

test_cabd_md5_SOURCES =	test/cabd_md5.c test/md5.c test/md5.h libmscab.la
test_cabd_md5_LDADD =	libmscabd.la

test_cabextract_md5_SOURCES = test/cabextract_md5.c test/md5.c test/md5.h

test_cabd_test_SOURCES=	test/cabd_test.c libmscab.la
test_cabd_test_LDADD = 	libmscabd.la

test_cabrip_SOURCES =	test/cabrip.c libmscab.la
test_cabrip_LDADD =	libmscabd.la

test_cabd_memory_SOURCES = test/cabd_memory.c libmscab.la
test_cabd_memory_LDADD = libmscabd.la

test_chminfo_SOURCES = test/chminfo.c libmschmd.la
test_chminfo_LDADD   = libmschmd.la
