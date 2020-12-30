cmake_minimum_required(VERSION 3.5)
project(external_memory_fractal_tree)

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

# include tlx, foxxll, stxxl
add_subdirectory(extlibs/stxxl/)
include_directories(extlibs/stxxl/include/)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${STXXL_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -Wextra -Wpedantic -ftemplate-depth=10000")

# enable CXX STANDARD
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# gtest
add_subdirectory(extlibs/googletest)
add_subdirectory(tests)

# executables
add_executable(run-fractal-tree run-fractal-tree.cpp include/fractal_tree/fractal_tree_cache.h)
add_executable(run-test run-test.cpp)

target_link_libraries(run-fractal-tree ${STXXL_LIBRARIES})
target_link_libraries(run-test ${STXXL_LIBRARIES})

