#!/bin/sh
# Runs the autoreconf tool, creating the configure script

autoreconf -i -W all
rc=$?; if [[ $rc != 0 ]]; then
  echo "Error: Failed to generate autojunk!"; exit $rc
else
  echo "You can now run ./configure"
fi
