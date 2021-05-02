# cabextract 1.9.1

<a href="https://github.com/kyz/libmspack/actions"><img src="https://github.com/kyz/libmspack/workflows/CMake%20Build%20Cabextract/badge.svg" height="18"></a>

A program to extract Microsoft Cabinet files.

(C) 2000-2019 Stuart Caie <kyzer@cabextract.org.uk>
This is free software with ABSOLUTELY NO WARRANTY.

Cabinet (.CAB) files are a form of archive, which Microsoft use to
distribute their software, and things like Windows Font Packs. The
cabextract program unpacks these files.

For more information, see https://www.cabextract.org.uk/
or run the command 'cabextract --help'.

Microsoft cabinet files should not be confused with InstallShield cabinet
files. InstallShield files are generally called "_sys.cab", "data1.hdr"
"data1.cab", "data2.cab" and so on, and are found in the same directory as
"setup.exe". They begin with the magic characters "ISc(" rather than
"MSCF". cabextract will print the message "This is probably an
InstallShield file." when it finds such a file. The file "doc/magic" in
the cabextract source archive includes additional file-identification
rules for the UNIX file(1) command, which distinguishes between Microsoft
and InstallShield cabinet files.

## Example usage

Extracting files from a cabinet file:

```sh
$ cabextract wibble.cab
```

Extracting files from an executable which contains a cabinet file:

```sh
$ cabextract wibble.exe
[cabextract will automatically search executables for embedded cabinets]
```

Extracting files from a set of cabinet files; wib01.cab, wib02.cab, ...:

```sh
$ cabextract wib01.cab
[cabextract will automatically get the names of the other files]
```

Extracting files to a directory of your choice (in this case, 'boogie'):

```sh
$ cabextract -d boogie wibble.cab
[cabextract will create the directory if it does not already exist]
```

Extracting files that match a filename pattern:

```sh
$ cabextract -F *.avi -F *.mpg movies.cab
```

Listing files from a cabinet file:

```sh
$ cabextract -l wibble.cab
```

Testing the integrity of a cabinet file, without extracting it:

```sh
$ cabextract -t wibble.cab
```

## BUILDING / INSTALLING

### Autotools

```sh
./configure
make
make install
```

This will install the cabextract program.
Some other libraries and executables are built, but not installed.

If building from the Git repository, running `rebuild.sh` will create all the
auto-generated files, then run `./configure && make`. Running `cleanup.sh` will
perform a thorough clean, deleting all auto-generated files.

In addition to gcc, you also need the following for building from repository:

- at least autoconf 2.57
- at least automake 1.11
- libtool

### CMake

cabextract can be compiled with the [CMake] build system.
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

#### Windows=specific build instructions

A build using the bundled mspack files won't work on Windows for a couple of reasons. Instead, you can build cabextract on Windows by first building libmspack and then build cabextract using the ENABLE_EXTERNAL_MSPACK option.

In the example below, we use build and link with the static libmspack library so we don't have to copy mspack.dll into the cabextract.exe directory.

In the libmspack directory:
```ps1
mkdir build ; if($?) {cd build}
cmake -DCMAKE_INSTALL_PREFIX:PATH=install .. -DENABLE_STATIC_LIB=ON
cmake --build . --config Release --target install
```

Then in the cabextract directory something like this*:
```ps1
mkdir build ; if($?) {cd build}
cmake .. -DENABLE_EXTERNAL_MSPACK=ON -DMSPack_INCLUDE_DIR="C:\Users\...\libmspack\build\install\include" -DMSPack_LIBRARY="C:\Users\...\libmspack\install\lib\mspack_static.lib"
cmake --build . --config Debug
.\Debug\cabextract.exe --help

*Important*: set the `MSPack_INCLUDE_DIR` and `MSPack_LIBRARY` variables to your mspack `include` directory and `mspack_static.lib` file
