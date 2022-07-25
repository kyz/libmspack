#
# Example executables
#
add_executable(cabd_memory)
target_sources(cabd_memory
    PRIVATE   cabd_memory.c)
target_link_libraries(cabd_memory mscabd_ObjLib)

add_executable(cabrip)
target_sources(cabrip
    PRIVATE   cabrip.c)
target_link_libraries(cabrip mspack_ObjLib)

add_executable(chmextract)
target_sources(chmextract
    PRIVATE   chmextract.c)
target_link_libraries(chmextract mspack_ObjLib)

add_executable(msexpand)
target_sources(msexpand
    PRIVATE   msexpand.c)
target_link_libraries(msexpand mspack_ObjLib)

add_executable(multifh)
target_sources(multifh
    PRIVATE   multifh.c)
target_link_libraries(multifh mscabd_ObjLib)

add_executable(oabextract)
target_sources(oabextract
    PRIVATE   oabextract.c)
target_link_libraries(oabextract mspack_ObjLib)
