function(wgrd_add_tests MODULE_NAME)
    cmake_parse_arguments(TESTS "" "" "SOURCES" ${ARGN})

    if(NOT TESTS_SOURCES)
        message(FATAL_ERROR "${MODULE_NAME} tests need SOURCES")
    endif()

    set(targetName "wgrd_${MODULE_NAME}_tests")

    add_executable(${targetName} ${TESTS_SOURCES})

    target_link_libraries(${targetName} PRIVATE
        wgrd_compiler_flags
        "wgrd_${MODULE_NAME}"
        Catch2::Catch2WithMain
    )

    set_target_properties(${targetName} PROPERTIES FOLDER "tests")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${TESTS_SOURCES})

    add_test(NAME ${MODULE_NAME} COMMAND ${targetName})
endfunction()
