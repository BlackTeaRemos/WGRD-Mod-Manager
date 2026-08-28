set(WGRD_COMMIT "" CACHE STRING "Commit identifier")
set(WGRD_BRANCH "" CACHE STRING "Branch or tag")
set(WGRD_SOURCE_REPOSITORY "" CACHE STRING "Source repository")
set(WGRD_RELEASE_REPOSITORY "" CACHE STRING "Release repository")
set(WGRD_INDEX_REPOSITORY "" CACHE STRING "Signed index repository")

function(wgrd_resolve_build_info)
    set(required
        WGRD_COMMIT
        WGRD_BRANCH
        WGRD_SOURCE_REPOSITORY
        WGRD_RELEASE_REPOSITORY
        WGRD_INDEX_REPOSITORY
    )

    set(missing "")
    foreach(name IN LISTS required)
        if("${${name}}" STREQUAL "")
            list(APPEND missing "${name}")
        endif()
    endforeach()

    if(missing)
        string(REPLACE ";" "\n  " listing "${missing}")
        message(FATAL_ERROR
            "build info not supplied\n"
            "  ${listing}\n"
            "pass every value with -D or a CMakeUserPresets entry")
    endif()

    string(SUBSTRING "${WGRD_COMMIT}" 0 7 shortCommit)
    set(WGRD_COMMIT "${shortCommit}" PARENT_SCOPE)
    set(WGRD_VERSION "${PROJECT_VERSION}" PARENT_SCOPE)
endfunction()

wgrd_resolve_build_info()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/BuildInfo.h.in"
    "${CMAKE_BINARY_DIR}/generated/domain/BuildInfo.h"
    @ONLY
)

message(STATUS "wgrd build ${PROJECT_VERSION} ${WGRD_COMMIT} ${WGRD_BRANCH}")
message(STATUS "wgrd source ${WGRD_SOURCE_REPOSITORY}")
message(STATUS "wgrd release ${WGRD_RELEASE_REPOSITORY}")
message(STATUS "wgrd index ${WGRD_INDEX_REPOSITORY}")
