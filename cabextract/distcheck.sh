#!/bin/sh
# checks the project can be distributed

./rebuild.sh && make distcheck &&
 make clean &&
 ./configure --with-external-libmspack && make && make -C test &&
 DISTCHECK_CONFIGURE_FLAGS=--with-external-libmspack make distcheck
