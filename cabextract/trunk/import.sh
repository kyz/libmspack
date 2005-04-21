#!/bin/sh
# this imports the latest libmspack CAB sources into cabextract

SRC=../libmspack/mspack

if [ ! -d mspack ]
then
  mkdir mspack
fi

for file in mspack.h system.c system.h cabd.c cab.h \
            lzxd.c lzx.h mszipd.c mszip.h qtmd.c qtm.h
do
  if [ -f mspack/$file ]
  then
    if [ $SRC/$file -nt mspack/$file ]; then cp $SRC/$file mspack; fi
  else
    cp $SRC/$file mspack
  fi
done

if [ -f mspack/ChangeLog ]
then
  if [ ../libmspack/ChangeLog -nt mspack/ChangeLog ]; then cp ../libmspack/ChangeLog mspack; fi
else
  cp ../libmspack/ChangeLog mspack
fi
