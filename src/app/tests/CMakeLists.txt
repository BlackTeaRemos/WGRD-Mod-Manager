find_package(Catch2 CONFIG REQUIRED)

add_executable(wgrd_integration_tests
    PublishInstallRoundTripTests.cpp
    PublishSeedingTests.cpp
)

target_link_libraries(wgrd_integration_tests PRIVATE
    wgrd_compiler_flags
    wgrd_manager
    wgrd_downloader
    Catch2::Catch2WithMain
)

set_target_properties(wgrd_integration_tests PROPERTIES FOLDER "tests")

add_test(NAME integration COMMAND wgrd_integration_tests)
