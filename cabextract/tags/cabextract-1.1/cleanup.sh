#!/bin/sh

chmod -R +w cabextract-0.* 2>/dev/null || true

rm -rf *.o aclocal.* autom4te.cache cabextract-0.* cabextract.spec config.* \
configure COPYING INSTALL install-sh libmspack.a Makefile Makefile.in missing \
mkinstalldirs mspack cabextract src/cabinfo stamp-h1 stamp-h1.in
