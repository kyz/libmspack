#!/bin/bash -eu

cd libmspack
./autogen.sh
./configure
make -j$(nproc)

find . -name "*.o" -exec ar rcs fuzz_lib.a {} \;
$CXX $CXXFLAGS -I./mspack -c $SRC/extract_fuzzer.c -o extract_fuzzer.o 
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE extract_fuzzer.o -o $OUT/extract_fuzzer fuzz_lib.a

zip $OUT/extract_fuzzer_seed_corpus.zip $SRC/libmspack/libmspack/test/test_files/cabd/partial_shortfile1.cab
