#!/bin/sh
./cleanup.sh
./import.sh
autoreconf -i -W all
./configure
make
make distcheck
