# Find all test source files (including subdirectories)
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer -std=c++23")

find_library(GPERFTOOLS_PROFILER profiler)
find_path(GPERFTOOLS_INCLUDE_DIR gperftools/profiler.h)
# Create an executable for each test file
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE ${GPERFTOOLS_INCLUDE_DIR})
    
    # Link cthsm library for cthsm tests, otherwise hsm
    if(${TEST_SOURCE} MATCHES "cthsm")
        target_link_libraries(${TEST_NAME} PRIVATE cthsm doctest ${GPERFTOOLS_PROFILER})
    else()
        target_link_libraries(${TEST_NAME} PRIVATE hsm doctest ${GPERFTOOLS_PROFILER})
    endif()
    
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
