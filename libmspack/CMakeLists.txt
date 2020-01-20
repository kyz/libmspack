cmake_minimum_required(VERSION 3.12)

project(libmspack
        VERSION 0.10.1
        DESCRIPTION "libmspack is a library that provides creation & extraction of various Microsoft compression and archive formats."
        LANGUAGES C)

# See versioning rule:
#  http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
#
# KEEP THESE IN SYNC WITH configure.ac
set(LT_CURRENT  1)
set(LT_REVISION 1)
set(LT_AGE      0)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
include(Version)

math(EXPR LT_SOVERSION "${LT_CURRENT} - ${LT_AGE}")
set(LT_VERSION "${LT_SOVERSION}.${LT_AGE}.${LT_REVISION}")
set(PACKAGE_VERSION ${PROJECT_VERSION})
HexVersion(PACKAGE_VERSION_NUM ${PROJECT_VERSION_MAJOR} ${PROJECT_VERSION_MINOR} ${PROJECT_VERSION_PATCH})

set(ENABLE_EXAMPLES_DEFAULT OFF)
set(ENABLE_DOCS_DEFAULT OFF)
include(CMakeOptions.txt)

if(ENABLE_LIB_ONLY AND (ENABLE_APP OR ENABLE_EXAMPLES))
    # Remember when disabled options are disabled for later diagnostics.
    set(ENABLE_LIB_ONLY_DISABLED_OTHERS 1)
else()
    set(ENABLE_LIB_ONLY_DISABLED_OTHERS 0)
endif()
if(ENABLE_LIB_ONLY)
    set(ENABLE_APP      OFF)
    set(ENABLE_EXAMPLES OFF)
endif()

# Do not disable assertions based on CMAKE_BUILD_TYPE.
foreach(_build_type Release MinSizeRel RelWithDebInfo)
    foreach(_lang C)
        string(TOUPPER CMAKE_${_lang}_FLAGS_${_build_type} _var)
        string(REGEX REPLACE "(^|)[/-]D *NDEBUG($|)" " " ${_var} "${${_var}}")
    endforeach()
endforeach()

# Support the latest c++ standard available.
include(ExtractValidFlags)
# Determine if _FILE_OFFSET_BITS 64 needs to be set to handle large files.
include(CheckFileOffsetBits)
# Define inline macro as needed.
include(TestInline)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the build type" FORCE)

    # Include "None" as option to disable any additional (optimization) flags,
    # relying on just CMAKE_C_FLAGS and CMAKE_CXX_FLAGS (which are empty by
    # default). These strings are presented in cmake-gui.
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        None Debug Release MinSizeRel RelWithDebInfo)
endif()

include(GNUInstallDirs)

# Always use '-fPIC'/'-fPIE' option.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Checks for header files.
include(CheckIncludeFile)
check_include_file(dlfcn.h      HAVE_DLFCN_H)
check_include_file(inttypes.h   HAVE_INTTYPES_H)
check_include_file(stdint.h     HAVE_STDINT_H)
check_include_file(stddef.h     HAVE_STDDEF_H)
check_include_file(limits.h     HAVE_LIMITS_H)
check_include_file(ctype.h      HAVE_CTYPE_H)
check_include_file(wctype.h     HAVE_WCTYPE_H)
check_include_file(errno.h      HAVE_ERRNO_H)
check_include_file(dirent.h     HAVE_DIRENT_H)
check_include_file(sys/types.h  HAVE_SYS_TYPES_H)
check_include_file(sys/stat.h   HAVE_SYS_STAT_H)
check_include_file(fnmatch.h    HAVE_FNMATCH_H)
check_include_file(iconv.h      HAVE_ICONV_H)
check_include_file(locale.h     HAVE_LOCALE_H)
check_include_file(stdarg.h     HAVE_STDARG_H)
check_include_file(stdlib.h     HAVE_STDLIB_H)
check_include_file(string.h     HAVE_STRING_H)
check_include_file(strings.h    HAVE_STRINGS_H)
check_include_file(sys/time.h   HAVE_SYS_TIME_H)
check_include_file(float.h      HAVE_FLOAT_H)
check_include_file(unistd.h     HAVE_UNISTD_H)

include(CheckIncludeFiles)
check_include_files("dlfcn.h;stdint.h;stddef.h;inttypes.h;stdlib.h;strings.h;string.h;float.h" StandardHeadersExist)
if(StandardHeadersExist)
    set(STDC_HEADERS 1 CACHE INTERNAL "System has ANSI C header files")
else()
    message(STATUS "ANSI C header files - not found")
    set(STDC_HEADERS 0 CACHE INTERNAL "System has ANSI C header files")
endif()


# Checks for library functions.
include(CheckFunctionExists)
check_function_exists(fseeko    HAVE_FSEEKO)
check_function_exists(mkdir     HAVE_MKDIR)
check_function_exists(_mkdir    HAVE__MKDIR)
check_function_exists(towlower  HAVE_TOWLOWER)

# Check size of types.
include(CheckTypeSize)
check_type_size("off_t" SIZEOF_OFF_T)
if(NOT SIZEOF_OFF_T)
    # Set it to "long int" to match the behavior of AC_TYPE_OFF_T (autotools).
    set(OFF_T_DEF "typedef int off_t;")
endif()

check_type_size("size_t" SIZEOF_SIZE_T)
if(NOT SIZEOF_SIZE_T)
    # Set it to "unsigned int" to match the behavior of AC_TYPE_SIZE_T (autotools).
    set(SIZE_T_DEF "typedef int size_t;")
endif()

check_type_size("ssize_t" SIZEOF_SSIZE_T)
if(NOT SIZEOF_SSIZE_T)
    # Set it to "int" to match the behavior of AC_TYPE_SSIZE_T (autotools).
    set(SSIZE_T_DEF "typedef int ssize_t;")
endif()

check_type_size("mode_t" SIZEOF_MODE_T)
if(NOT SIZEOF_MODE_T)
    # Set it to "int" to match the behavior of AC_TYPE_MODE_T (autotools).
    set(MODE_T_DEF "typedef int mode_t;")
endif()

# Compile tests
try_compile(MKDIR_TAKES_ONE_ARG ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake/compiletest_mkdir.c)

# Check if big-endian
include(TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

set(WARNCFLAGS)
if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    if(ENABLE_WERROR)
        set(WARNCFLAGS    /WX)
    endif()
else()
    if(ENABLE_WERROR)
        extract_valid_c_flags(WARNCFLAGS    -Werror)
    endif()

    # For C compiler
    extract_valid_c_flags(WARNCFLAGS
        -Wall
        -Wextra
        -Wmissing-prototypes
        -Wstrict-prototypes
        -Wmissing-declarations
        -Wpointer-arith
        -Wdeclaration-after-statement
        -Wformat-security
        -Wwrite-strings
        -Wshadow
        -Winline
        -Wnested-externs
        -Wfloat-equal
        -Wundef
        -Wendif-labels
        -Wempty-body
        -Wcast-align
        -Wclobbered
        -Wvla
        -Wpragmas
        -Wunreachable-code
        -Waddress
        -Wattributes
        -Wdiv-by-zero
        -Wshorten-64-to-32
        -Wconversion
        -Wextended-offsetof
        -Wformat-nonliteral
        -Wlanguage-extension-token
        -Wmissing-field-initializers
        -Wmissing-noreturn
        -Wmissing-variable-declarations
        # -Wpadded                          # Not used because we cannot change public structs
        -Wsign-conversion
        # -Wswitch-enum                     # Not used because this basically disallows default case
        -Wunreachable-code-break
        -Wunused-macros
        -Wunused-parameter
        -Wredundant-decls
        -Wheader-guard
        #-Wno-format-nonliteral              # This is required because we pass format string as "const char*.
        -Wno-unused-parameter
        -Wno-unused-result
    )
endif()

if(ENABLE_DEBUG)
    set(DEBUGBUILD 1)
endif()

# autotools-compatible names
# Sphinx expects relative paths in the .rst files. Use the fact that the files
# below are all one directory level deep.
file(RELATIVE_PATH top_srcdir   ${CMAKE_CURRENT_BINARY_DIR}/dir ${CMAKE_CURRENT_SOURCE_DIR})
file(RELATIVE_PATH top_builddir ${CMAKE_CURRENT_BINARY_DIR}/dir ${CMAKE_CURRENT_BINARY_DIR})
set(abs_top_srcdir   ${CMAKE_CURRENT_SOURCE_DIR})
set(abs_top_builddir ${CMAKE_CURRENT_BINARY_DIR})
# libmspack.pc (pkg-config file)
set(prefix      ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(bindir      ${CMAKE_INSTALL_FULL_BINDIR})
set(sbindir     ${CMAKE_INSTALL_FULL_SBINDIR})
set(libdir      ${CMAKE_INSTALL_FULL_LIBDIR})
set(includedir  ${CMAKE_INSTALL_FULL_INCLUDEDIR})
set(VERSION     ${PACKAGE_VERSION})

# Generate config.h
add_definitions(-DHAVE_CONFIG_H)
configure_file(config.h.in.cmake config.h)
include_directories(${PROJECT_BINARY_DIR})

# Generate, install pkg-config file.
configure_file(
    libmspack.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libmspack.pc
    @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libmspack.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

#
# The build targets.
#
add_subdirectory(mspack)

if(ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

enable_testing()
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
add_subdirectory(test)

if(ENABLE_DOCS)
    add_subdirectory(doc)
endif()

#
# The Summary Info.
#
string(TOUPPER "${CMAKE_BUILD_TYPE}" _build_type)
message(STATUS "Summary of build options:

    Package version: ${VERSION}
    Library version: ${LT_CURRENT}:${LT_REVISION}:${LT_AGE}
    Install prefix:  ${CMAKE_INSTALL_PREFIX}
    Target system:   ${CMAKE_SYSTEM_NAME}
    Compiler:
        Build type:     ${CMAKE_BUILD_TYPE}
        C compiler:     ${CMAKE_C_COMPILER}
        CFLAGS:         ${CMAKE_C_FLAGS_${_build_type}} ${CMAKE_C_FLAGS}
        WARNCFLAGS:     ${WARNCFLAGS}
    Docs:
        Build docs:     ${ENABLE_DOCS}
    Features:
        Static library: ${ENABLE_STATIC_LIB}
        Examples:       ${ENABLE_EXAMPLES}
")
