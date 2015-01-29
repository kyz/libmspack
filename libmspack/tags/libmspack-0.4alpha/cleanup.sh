#!/bin/sh
topdir=`pwd`
for x in . doc test
do
  cd $x
  chmod -R a+rwx `svn pg svn:ignore .` 2>/dev/null
  rm -vrf `svn pg svn:ignore .`
  cd $topdir
done
