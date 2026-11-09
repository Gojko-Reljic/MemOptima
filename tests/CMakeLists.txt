add_executable(memoptima_core_tests
    application_info_test.cpp
    linux/meminfo_parser_test.cpp
)

target_include_directories(memoptima_core_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(memoptima_core_tests
    PRIVATE
        memoptima_build_options
        memoptima_core
        GTest::gtest_main
)

set_target_properties(memoptima_core_tests PROPERTIES
    CXX_EXTENSIONS OFF
)

include(GoogleTest)
gtest_discover_tests(memoptima_core_tests DISCOVERY_MODE PRE_TEST)
