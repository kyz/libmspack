#!/bin/sh
./cleanup.sh && autoreconf -i -W all && ./configure && make && make distcheck 
