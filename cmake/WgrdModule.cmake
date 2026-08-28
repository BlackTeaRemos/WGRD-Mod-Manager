set(WGRD_FOUNDATION_MODULE "domain")
set(WGRD_COMPOSITION_MODULE "app")

function(wgrd_add_module MODULE_NAME)
    cmake_parse_arguments(MODULE "EXECUTABLE" "" "SOURCES;DEPS" ${ARGN})

    if(NOT MODULE_SOURCES)
        message(FATAL_ERROR "${MODULE_NAME} needs SOURCES")
    endif()

    __WgrdAssertDependenciesAllowed("${MODULE_NAME}" "${MODULE_DEPS}")

    set(targetName "wgrd_${MODULE_NAME}")

    if(MODULE_EXECUTABLE)
        add_executable(${targetName} ${MODULE_SOURCES})
    else()
        add_library(${targetName} STATIC ${MODULE_SOURCES})
    endif()

    target_include_directories(${targetName} PUBLIC "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(${targetName} PRIVATE wgrd_compiler_flags)

    foreach(dependency IN LISTS MODULE_DEPS)
        target_link_libraries(${targetName} PUBLIC "wgrd_${dependency}")
    endforeach()

    set_target_properties(${targetName} PROPERTIES FOLDER "modules")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${MODULE_SOURCES})
endfunction()

function(__WgrdAssertDependenciesAllowed MODULE_NAME MODULE_DEPS)
    if(MODULE_NAME STREQUAL WGRD_COMPOSITION_MODULE)
        return()
    endif()

    if(MODULE_NAME STREQUAL WGRD_FOUNDATION_MODULE AND MODULE_DEPS)
        message(FATAL_ERROR "${MODULE_NAME} allows no dependency")
    endif()

    foreach(dependency IN LISTS MODULE_DEPS)
        if(NOT dependency STREQUAL WGRD_FOUNDATION_MODULE)
            message(FATAL_ERROR "${MODULE_NAME} rejects ${dependency}")
        endif()
    endforeach()
endfunction()
