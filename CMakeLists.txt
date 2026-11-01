cmake_minimum_required(VERSION 3.20)

project(MemOptima VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(MEMOPTIMA_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer." OFF)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "MemOptima currently supports GCC and Clang only.")
endif()

add_library(memoptima_build_options INTERFACE)

target_compile_features(memoptima_build_options INTERFACE cxx_std_23)
target_compile_options(memoptima_build_options INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

if(MEMOPTIMA_ENABLE_SANITIZERS)
    target_compile_options(memoptima_build_options INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(memoptima_build_options INTERFACE
        -fsanitize=address,undefined
    )
endif()

configure_file(
    cmake/version.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/include/memoptima/version.hpp"
    @ONLY
)

add_subdirectory(src)

if(BUILD_TESTING)
    include(FetchContent)

    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG 063de7e9578f82b369302001269680b4b1553359
    )
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()
