#
# mspack targets
#

# The bundled mscabd OBJECT-library
add_library(mscabd_ObjLib OBJECT)
target_sources(mscabd_ObjLib
    PRIVATE
        system.h   system.c
        cab.h      cabd.c
        lzx.h      lzxd.c
        mszip.h    mszipd.c
        qtm.h      qtmd.c
        readbits.h readhuff.h
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/mspack.h
)
target_include_directories(mscabd_ObjLib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# The bundled mscabd static library.
# We'll call it mspack, though it only provides cabd features.
# It's enough for cabextract.
add_library(mspack_static STATIC)
target_sources(mspack_static
    PRIVATE
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/mspack.h
)
target_link_libraries(mspack_static PRIVATE mscabd_ObjLib)
set_target_properties(mspack_static PROPERTIES
    COMPILE_FLAGS       "${WARNCFLAGS}"
    ARCHIVE_OUTPUT_NAME mspack_static)
target_compile_definitions(mspack_static PUBLIC MSPACK_STATICLIB)

# Alias this as MSPack::mspack for cabextract
add_library( MSPack::mspack ALIAS mspack_static )
