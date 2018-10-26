#!/bin/sh
# deletes all auto-generated / compiled files

topdir="`pwd`"
for x in . mspack src test
do
  cd "$topdir/$x"
  chmod -R a+rwx `cat .gitignore` 2>/dev/null
  rm -vrf `cat .gitignore`
done
