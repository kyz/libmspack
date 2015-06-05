#!/bin/sh
topdir=`pwd`
for x in . mspack src
do
  cd $x
  chmod -R a+rwx `cat .gitignore` 2>/dev/null
  rm -vrf `cat .gitignore`
  cd $topdir
done
