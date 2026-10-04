cmake_minimum_required(VERSION 3.16)
project(libmspack C)

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)

check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(limits.h HAVE_LIMITS_H)
check_symbol_exists(fseeko "stdio.h" HAVE_FSEEKO)
check_symbol_exists(towlower "wctype.h" HAVE_TOWLOWER)

# Prefer 64-bit off_t where the platform supports it (AC_SYS_LARGEFILE).
check_type_size("off_t" SIZEOF_OFF_T)
if(SIZEOF_OFF_T AND SIZEOF_OFF_T LESS 8)
  set(_mspack_saved_required_defs "${CMAKE_REQUIRED_DEFINITIONS}")
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_FILE_OFFSET_BITS=64)
  unset(SIZEOF_OFF_T CACHE)
  unset(HAVE_SIZEOF_OFF_T CACHE)
  check_type_size("off_t" SIZEOF_OFF_T)
  set(CMAKE_REQUIRED_DEFINITIONS "${_mspack_saved_required_defs}")
  if(SIZEOF_OFF_T EQUAL 8)
    set(_FILE_OFFSET_BITS 64)
  endif()
endif()
if(NOT SIZEOF_OFF_T)
  set(SIZEOF_OFF_T 0)
endif()

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_library(mspack
  mspack/system.c
  mspack/cabc.c
  mspack/cabd.c
  mspack/chmc.c
  mspack/chmd.c
  mspack/hlpc.c
  mspack/hlpd.c
  mspack/litc.c
  mspack/litd.c
  mspack/kwajc.c
  mspack/kwajd.c
  mspack/szddc.c
  mspack/szddd.c
  mspack/oabc.c
  mspack/oabd.c
  mspack/lzxc.c
  mspack/lzxd.c
  mspack/mszipc.c
  mspack/mszipd.c
  mspack/qtmd.c
  mspack/lzssd.c
  mspack/crc32.c
)
add_library(libmspack::mspack ALIAS mspack)

set_target_properties(mspack PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(mspack PRIVATE HAVE_CONFIG_H)
if(_FILE_OFFSET_BITS)
  # Public so callers share the same off_t width as the library (MSPACK_SYS_SELFTEST).
  target_compile_definitions(mspack PUBLIC _FILE_OFFSET_BITS=${_FILE_OFFSET_BITS})
endif()

target_include_directories(mspack
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/mspack
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
