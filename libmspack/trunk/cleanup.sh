#!/bin/sh

chmod -R +w lib* 2>/dev/null || true

rm -rf .libs .deps *.o *.lo *.loT aclocal.* autom4te.cache config.* \
configure depcomp INSTALL install-sh libtool ltmain.sh lib* Makefile \
Makefile.in missing mkinstalldirs stamp-h1 stamp-h1.in test/chmx \
test/chmd_md5 test/cabd_md5 test/cabextract_md5 test/cabrip test/cabd_test \
test/.libs test/.dirstamp
