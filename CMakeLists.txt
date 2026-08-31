cmake_minimum_required(VERSION 3.28)

project(WgrdModManager
    VERSION 0.6.1
    DESCRIPTION "Mod manager for Wargame Red Dragon"
    LANGUAGES CXX
)

option(WGRD_BUILD_TESTS "Build test targets" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_EXTENSIONS OFF)

include(WgrdBuildInfo)
include(WgrdRegistry)
include(WgrdCompilerFlags)
include(WgrdModule)

if(WGRD_BUILD_TESTS)
    enable_testing()
    find_package(Catch2 CONFIG REQUIRED)
    include(WgrdTests)
endif()

add_subdirectory(src)
