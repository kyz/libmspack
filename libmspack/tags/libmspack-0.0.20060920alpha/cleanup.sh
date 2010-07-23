#!/bin/sh
topdir=`pwd`
for x in `find . -name .cvsignore`
do
  cd `dirname $x`
  chmod -R a+rwx `cat .cvsignore` 2>/dev/null
  rm -vrf `cat .cvsignore`
  cd $topdir
done
