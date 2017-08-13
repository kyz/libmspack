#!/bin/sh
# rebuilds the entire project

./cleanup.sh && ./autogen.sh && ./configure && make

# and to build the API docs: make -C doc 
# and before any release: make distcheck
