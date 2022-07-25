find_package(Doxygen REQUIRED)

set(DOXYGEN_GENERATE_HTML       YES)
set(DOXYGEN_GENERATE_MAN        NO)
set(DOXYGEN_OUTPUT_DIRECTORY    ${CMAKE_CURRENT_BINARY_DIR})

set(EXTRACT_ALL                 YES)
set(HIDE_UNDOC_MEMBERS          YES)
set(JAVADOC_AUTOBRIEF           YES)
set(OPTIMIZE_OUTPUT_FOR_C       YES)
set(FULL_PATH_NAMES             NO)
set(GENERATE_LATEX              NO)

doxygen_add_docs(
    doxygen
    ../mspack/mspack.h
    COMMENT "Generate html documentation"
)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION ${CMAKE_INSTALL_DOCDIR})
