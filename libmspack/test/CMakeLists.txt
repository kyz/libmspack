#
# The md5 OBJECT-library, required for some tests
#
add_library(md5_ObjLib OBJECT)
target_sources(md5_ObjLib
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/md5.c
    PUBLIC    ${CMAKE_CURRENT_SOURCE_DIR}/md5.h
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/md5_fh.h)
target_include_directories(md5_ObjLib
    PRIVATE
        ${PROJECT_SOURCE_DIR}
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR})

#
# Test executables, run with `ctest`
#
add_executable(cabd_test)
target_sources(cabd_test
    PRIVATE   cabd_test.c)
target_link_libraries(cabd_test
    PRIVATE   mscabd_ObjLib md5_ObjLib)
target_include_directories(cabd_test PRIVATE ${PROJECT_SOURCE_DIR})
set_target_properties(cabd_test PROPERTIES
    COMPILE_DEFINITIONS TEST_FILES=${CMAKE_CURRENT_SOURCE_DIR}/test_files/cabd)
add_test(NAME "cabd_test" COMMAND cabd_test)

add_executable(chmd_test)
target_sources(chmd_test
    PRIVATE   chmd_test.c)
target_link_libraries(chmd_test
    PRIVATE   mschmd_ObjLib md5_ObjLib)
target_include_directories(chmd_test PRIVATE ${PROJECT_SOURCE_DIR})
set_target_properties(chmd_test PROPERTIES
    COMPILE_DEFINITIONS TEST_FILES=${CMAKE_CURRENT_SOURCE_DIR}/test_files/chmd)
add_test(NAME "chmd_test" COMMAND chmd_test)

add_executable(kwajd_test)
target_sources(kwajd_test
    PRIVATE   kwajd_test.c)
target_link_libraries(kwajd_test
    PRIVATE   mspack_ObjLib)
target_include_directories(kwajd_test PRIVATE ${PROJECT_SOURCE_DIR})
set_target_properties(kwajd_test PROPERTIES
    COMPILE_DEFINITIONS TEST_FILES=${CMAKE_CURRENT_SOURCE_DIR}/test_files/kwajd)
add_test(NAME "kwajd_test" COMMAND kwajd_test)

#
# Other test executables
#
add_executable(cabd_md5)
target_sources(cabd_md5
    PRIVATE   cabd_md5.c)
target_link_libraries(cabd_md5
    PRIVATE   mscabd_ObjLib md5_ObjLib)
if(WIN32)
    target_include_directories(cabd_md5
        PRIVATE
            ${PROJECT_SOURCE_DIR}
            win32)
    target_sources(cabd_md5 PRIVATE win32/dirent.h)
    set_target_properties(cabd_md5 PROPERTIES
        COMPILE_DEFINITIONS HAVE_DIRENT_H)
endif()

add_executable(chmd_find)
target_sources(chmd_find
    PRIVATE   chmd_find.c)
target_link_libraries(chmd_find
    PRIVATE   mschmd_ObjLib)
target_include_directories(chmd_find PRIVATE ${PROJECT_SOURCE_DIR})

add_executable(chmd_md5)
target_sources(chmd_md5
    PRIVATE   chmd_md5.c)
target_link_libraries(chmd_md5
    PRIVATE   mschmd_ObjLib md5_ObjLib)
target_include_directories(chmd_md5 PRIVATE ${PROJECT_SOURCE_DIR})

add_executable(chmd_order)
target_sources(chmd_order
    PRIVATE   chmd_order.c)
target_link_libraries(chmd_order
    PRIVATE   mschmd_ObjLib md5_ObjLib)
target_include_directories(chmd_order PRIVATE ${PROJECT_SOURCE_DIR})

add_executable(chminfo)
target_sources(chminfo
    PRIVATE   chminfo.c)
target_link_libraries(chminfo
    PRIVATE   mschmd_ObjLib)
target_include_directories(chminfo PRIVATE ${PROJECT_SOURCE_DIR})
