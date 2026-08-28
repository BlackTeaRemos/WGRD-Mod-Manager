wgrd_add_module(app
    EXECUTABLE
    SOURCES
        Main.cpp
    DEPS
        domain
        manager
        downloader
        gui
)

set_target_properties(wgrd_app PROPERTIES
    OUTPUT_NAME "wgrd-mod-manager"
    WIN32_EXECUTABLE TRUE
    FOLDER ""
)

