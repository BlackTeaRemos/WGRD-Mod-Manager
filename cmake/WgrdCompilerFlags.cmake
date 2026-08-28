add_library(wgrd_compiler_flags INTERFACE)

target_compile_options(wgrd_compiler_flags INTERFACE
    /std:c++latest
    /Zc:__cplusplus
    /Zc:preprocessor
    /permissive-
    /utf-8
    /W4
    /WX
)

target_compile_definitions(wgrd_compiler_flags INTERFACE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
