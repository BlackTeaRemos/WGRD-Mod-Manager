file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/wgrd-mod-manager.ico" WGRD_ICON_PATH)
string(REPLACE "\\" "\\\\" WGRD_ICON_PATH "${WGRD_ICON_PATH}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppResources.rc.in"
    "${CMAKE_BINARY_DIR}/generated/app/AppResources.rc"
    @ONLY
)

wgrd_add_module(app
    EXECUTABLE
    SOURCES
        Main.cpp
        session/GameLocationRequest.h
        session/GameLocationRequest.cpp
        session/ManagerSession.h
        session/ManagerSession.cpp
    DEPS
        domain
        manager
        downloader
        gui
)

target_sources(wgrd_app PRIVATE "${CMAKE_BINARY_DIR}/generated/app/AppResources.rc")

set_target_properties(wgrd_app PROPERTIES
    OUTPUT_NAME "wgrd-mod-manager"
    WIN32_EXECUTABLE TRUE
    FOLDER ""
)

if(WGRD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
