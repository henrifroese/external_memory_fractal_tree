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

# executables
add_executable(run-fractal-tree run-fractal-tree.cpp)
target_link_libraries(run-fractal-tree ${STXXL_LIBRARIES})

# executables
add_executable(run-frage frage.cpp)
target_link_libraries(run-frage ${STXXL_LIBRARIES})