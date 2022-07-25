# libmspack 0.10.1alpha

<a href="https://github.com/kyz/libmspack/actions"><img src="https://github.com/kyz/libmspack/workflows/CMake%20Build%20Libmspack/badge.svg" height="18"></a>

The purpose of libmspack is to provide compressors and decompressors,
archivers and dearchivers for Microsoft compression formats: CAB, CHM, WIM,
LIT, HLP, KWAJ and SZDD. It is also designed to be easily embeddable,
stable, robust and resource-efficient.

The library is not intended as a generalised "any archiver" interface.
Users of the library must explicitly choose the format they intend to work
with.

All special features of the above formats will be covered as fully as
possible -- for example, CAB's multi-part cabinet sets, or CHM's fast
lookup indices. All compression methods used by the formats will be
implemented as completely as possible.

However, other than what is required for access to these formats and their
features, no other functionality is intended. There is no file metadata
translation functionality. All file I/O is abstracted, although a default
implementation using the standard C library is provided.


## DOCUMENTATION

The API documentation is stored in the doc/ directory. It is generated
automatically from mspack.h with doxygen. It is also available online at
https://www.cabextract.org.uk/libmspack/doc/


## BUILDING / INSTALLING

### Autotools

```sh
./configure
make
make install
```

This will install the main libmspack library and mspack.h header file.
Some other libraries and executables are built, but not installed.

If building from the Git repository, running `rebuild.sh` will create all the
auto-generated files, then run `./configure && make`. Running `cleanup.sh` will
perform a thorough clean, deleting all auto-generated files.

In addition to gcc, you also need the following for building from repository:

- at least autoconf 2.57
- at least automake 1.11
- libtool

This is an alpha release. Unless you are in a position to package the
libmspack library for the environment you intend to run your application,
it is recommended that you do not rely on users of your software having
the binary library installed and instead you should include the libmspack
source files directly in your application's build environment.

### CMake

libmspack can be compiled with the [CMake] build system.
The following instructions recommend compiling in a `build` subdirectory.

#### Basic Release build

```sh
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

#### Basic Debug build

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE="Debug"
cmake --build . --config Debug
```

#### Build and install to a specific install location (prefix)

```sh
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/install ..
cmake --build . --target install
```

Windows (Powershell)
```ps1
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX:PATH=$(Get-Location)/install
cmake --build . --target install
```

#### Build with example applications

```sh
mkdir build && cd build
cmake .. -DENABLE_EXAMPLES=ON
cmake --build .
```

#### Build and generate html documentation

```sh
mkdir build && cd build
cmake .. -DENABLE_DOCS=ON
cmake --build . --target doxygen
```

#### Build and run tests

- `-V`: Verbose
- `-C`: Required for Windows builds

```sh
mkdir build && cd build
cmake ..
cmake --build . --config Debug
ctest -C Debug -V
```

Or try `ctest -C Debug -VV --output-on-failure` for extra verbose output.

#### Build, test, and install in Release mode

```sh
mkdir build && cd build
cmake .. -DENABLE_EXAMPLES=ON -DENABLE_STATIC_LIB=ON -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/install -DENABLE_DOCS=ON
cmake --build . --config Release
ctest -C Release -V
cmake --build . --config Release --target install doxygen
```

Windows (Powershell):
```ps1
mkdir build ; if($?) {cd build}
cmake .. -DENABLE_EXAMPLES=ON -DENABLE_STATIC_LIB=ON -DCMAKE_INSTALL_PREFIX:PATH=$(Get-Location)/install
cmake --build . --config Release
ctest -C Release -V
cmake --build . --config Release --target install
```

## LEGAL ISSUES

To the best of my knowledge, libmspack does not infringe on any
compression or decompression patents. However, this is not legal
advice, and it is recommended that you perform your own patent search.

libmspack is licensed under the LGPL - see COPYING.LIB in this directory.

The LGPL requires you to build libmspack as a stand alone library then link
your code to it using a linker. I personally grant you some extra rights:
you can incorporate libmspack's source code wholly or partially in your own
code, without having to build and link libmspack as an independent library,
provided you meet ALL of the following conditions:

1. ANY modifications to the existing libmspack source code are published and
   distributed under the LGPL license.
2. You MUST NOT use libmspack function calls, structures or definitions unless
   they are defined in the public library interface "mspack.h".
3. When distributing your code, you MUST make clear your code uses libmspack,
   and either include the full libmspack distribution with your code, or
   provide access to it as per clause 4 of the LGPL.

## EXAMPLE CODE

libmspack is bundled with programs which demonstrate the library's features.

| Program                | Description
:------------------------|:------------------------------------------------------
| examples/cabd_memory.c | an mspack_system that can read and write to memory
| examples/multifh.c     | an mspack_system that can simultaneously work on
|                        | in-memory images, raw file descriptors, open file
|                        | handles and regular disk files
| examples/cabrip.c      | extracts any CAB files embedded in another file
| examples/chmextract.c  | extracts all files in a CHM file to disk
| examples/msexpand.c    | expands an SZDD or KWAJ file
| examples/oabextract.c  | extracts an Exchange Offline Address Book (.LZX) file
|                        |
| test/cabd_c10          | tests the CAB decompressor on the C10 collection
| test/cabd_compare      | compares libmspack with Microsoft's EXTRACT/EXPAND.EXE
| test/cabd_md5          | shows MD5 checksums of all files in a CAB file/set
| test/chmd_compare      | compares libmspack with Microsoft's HH.EXE
| test/chmd_find.c       | checks all files in a CHM file can be fast-found
| test/chmd_md5.c        | shows MD5 checksums of all files within a CHM file
| test/chmd_order.c      | extracts files in a CHM file in four different ways
| test/chminfo.c         | prints verbose information about CHM file structures
| test/msdecompile_md5   | runs Microsoft's HH.EXE -DECOMPILE via WINE
| test/msexpand_md5      | runs Microsoft's EXPAND.EXE via WINE
| test/msextract_md5     | runs Microsoft's EXTRACT.EXE via WINE

Here is a simple example of usage, which will create a CAB decompressor,
then use that to open an existing Microsoft CAB file called "example.cab",
and list the names of all the files contained in that cab.

```c
#include <stdio.h>
#include <unistd.h>
#include "mspack.h"

int main() {
  struct mscab_decompressor *cabd;
  struct mscabd_cabinet *cab;
  struct mscabd_file *file;
  int test;

  MSPACK_SYS_SELFTEST(test);
  if (test != MSPACK_ERR_OK) exit(0);

  if ((cabd = mspack_create_cab_decompressor(NULL))) {
    if ((cab = cabd->open(cabd, "example.cab"))) {
      for (file = cab->files; file; file = file->next) {
        printf("%s\n", file->filename);
      }
      cabd->close(cabd, cab);
    }
    mspack_destroy_cab_decompressor(cabd);
  }
  return 0;
}
```
