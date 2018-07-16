#!/bin/sh
topdir=`pwd`
for x in . mspack src
do
  files=`cat .gitignore | sed 's!^/!!'`
  cd $x
  chmod -R a+rwx $files 2>/dev/null
  rm -vrf $files
  cd $topdir
done
