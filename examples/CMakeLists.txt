# Find all test source files
# file(GLOB EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
# set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer  -std=c++23")

# find_library(GPERFTOOLS_PROFILER profiler)
# find_library(GPERFTOOLS_TCMALLOC tcmalloc)
# find_path(GPERFTOOLS_INCLUDE_DIR gperftools/profiler.h)
# # Create an executable for each test file
# foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
#     get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
#     add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
#     target_include_directories(${EXAMPLE_NAME} PRIVATE ${GPERFTOOLS_INCLUDE_DIR})
#     target_link_libraries(${EXAMPLE_NAME} PRIVATE hsm doctest ${GPERFTOOLS_PROFILER} ${GPERFTOOLS_TCMALLOC})
# endforeach()

add_executable(cthsm_example cthsm_example.cpp)
target_link_libraries(cthsm_example PRIVATE cthsm)