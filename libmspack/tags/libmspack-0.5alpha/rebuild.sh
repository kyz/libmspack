#!/bin/sh
./cleanup.sh
mkdir m4
autoreconf -i -W all
./configure
make
make -C doc
make distcheck
