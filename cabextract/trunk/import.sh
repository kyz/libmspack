#!/bin/sh
# this imports the latest libmspack CAB sources into cabextract

SRC=../libmspack/mspack

if [ ! -d mspack ]
then
  mkdir mspack
fi

cd mspack

for file in mspack.h system.c system.h cabd.c cab.h \
            lzxd.c lzx.h mszipd.c mszip.h qtmd.c qtm.h
do
  if [ -f $file ]
  then
    if [ $SRC/$file -nt $file ]; then cp $SRC/$file .; fi
  else
    cp $SRC/$file .
  fi
done
