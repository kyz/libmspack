if(NOT WIN32)

    set(abs_srcdir   ${CMAKE_CURRENT_SOURCE_DIR})
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/testcase.in
        ${CMAKE_CURRENT_BINARY_DIR}/test/testcase
    )
    # file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bugs
    #     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    # file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cabs
    #     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

    find_program (BASH_PROGRAM bash)

    if (BASH_PROGRAM)
        add_test (bugs            ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/bugs.test)
        add_test (case-ascii      ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/case-ascii.test)
        add_test (dirwalk-vulns   ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/dirwalk-vulns.test)
        add_test (encoding        ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/encoding.test)
        add_test (large-files     ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/large-files.test)
        add_test (mixed           ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/mixed.test)
        add_test (search          ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/search.test)
        add_test (simple          ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/simple.test)
        add_test (split           ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/split.test)
        add_test (utf8-stresstest ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/utf8-stresstest.test)
    endif (BASH_PROGRAM)

endif()
