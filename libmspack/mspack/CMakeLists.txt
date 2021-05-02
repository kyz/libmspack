#
# mspack targets
#
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/mspack-version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/mspack-version.h
)

# The mspack OBJECT-library
add_library(mspack_ObjLib OBJECT)
target_sources(mspack_ObjLib
    PRIVATE
        system.h   system.c
        cab.h      cabc.c     cabd.c
        chm.h      chmc.c     chmd.c
        hlp.h      hlpc.c     hlpd.c
        lit.h      litc.c     litd.c
        kwaj.h     kwajc.c    kwajd.c
        szdd.h     szddc.c    szddd.c
        oab.h      oabc.c     oabd.c
        lzx.h      lzxc.c     lzxd.c
        mszip.h    mszipc.c   mszipd.c
        qtm.h      qtmd.c
        readbits.h readhuff.h
        lzss.h     lzssd.c
        des.h      sha.h
        crc32.c    crc32.h
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/mspack.h
        ${CMAKE_CURRENT_BINARY_DIR}/mspack-version.h
)
target_include_directories(mspack_ObjLib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# The mscabd OBJECT-library
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
        ${CMAKE_CURRENT_BINARY_DIR}/mspack-version.h
)
target_include_directories(mscabd_ObjLib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# The mschmd OBJECT-library
add_library(mschmd_ObjLib OBJECT)
target_sources(mschmd_ObjLib
    PRIVATE
        system.h   system.c
        chm.h      chmd.c
        lzx.h      lzxd.c
        readbits.h readhuff.h
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/mspack.h
        ${CMAKE_CURRENT_BINARY_DIR}/mspack-version.h
)
target_include_directories(mschmd_ObjLib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Windows resource file
set(MSPACK_RES "")
if(WIN32)
    configure_file(
        version.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
        @ONLY)

    set(MSPACK_RES ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

if(ENABLE_SHARED_LIB)
    # The libmspack shared library.
    add_library(mspack SHARED ${MSPACK_RES})
    target_sources(mspack
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/mspack.def
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/mspack.h
            ${CMAKE_CURRENT_BINARY_DIR}/mspack-version.h
    )
    target_link_libraries(mspack PRIVATE mspack_ObjLib)
    set_target_properties(mspack PROPERTIES
        COMPILE_FLAGS "${WARNCFLAGS}"
        VERSION ${LT_VERSION} SOVERSION ${LT_SOVERSION})
    if(WIN32)
        set_target_properties(mspack PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
    install(TARGETS mspack DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(
        FILES
            mspack.h
            ${CMAKE_CURRENT_BINARY_DIR}/mspack-version.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    add_library( MSPack::mspack ALIAS mspack )
endif()

if(ENABLE_STATIC_LIB)
    # The libmspack static library.
    add_library(mspack_static STATIC)
    target_sources(mspack_static
        PRIVATE
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/mspack.h
            ${CMAKE_CURRENT_BINARY_DIR}/mspack-version.h
    )
    target_link_libraries(mspack_static PRIVATE mspack_ObjLib)
    set_target_properties(mspack_static PROPERTIES
        COMPILE_FLAGS       "${WARNCFLAGS}"
        VERSION             ${LT_VERSION}
        SOVERSION           ${LT_SOVERSION}
        ARCHIVE_OUTPUT_NAME mspack_static)
    target_compile_definitions(mspack_static PUBLIC MSPACK_STATICLIB)
    install(TARGETS mspack_static DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(
        FILES
            mspack.h
            ${CMAKE_CURRENT_BINARY_DIR}/mspack-version.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    add_library( MSPack::mspack_static ALIAS mspack_static )
endif()
