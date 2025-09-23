cmake_minimum_required(VERSION 3.15)
project(hsm VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_MAKEFILE ON)
# Add compiler-specific flags
if(MSVC)
    add_compile_options(/W4 /WX /GR- /std:c++23 /permissive- /Zc:preprocessor /Zc:__cplusplus)
else()
    add_compile_options(
        -Wall 
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wcast-qual
        -Wformat=2
        -Wunused
        -Wnull-dereference
        -Wdouble-promotion
        -Wold-style-cast
        -fno-rtti
        -std=c++23
    )
endif()

include(FetchContent)
# Find required packages

FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.0
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)



# Add header-only library
add_library(hsm INTERFACE)

# target_include_directories(hsm INTERFACE ${spdlog_SOURCE_DIR}/include)
target_include_directories(hsm INTERFACE ${doctest_SOURCE_DIR})

# Set include directories
target_include_directories(hsm INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


# Add tests if building with tests enabled
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

# Add examples if building with examples enabled
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
