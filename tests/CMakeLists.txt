enable_testing()

# retrieve tests, all *.cpp files in ./tests folder
file(GLOB TEST_SOURCES Test*.cpp)
add_executable(fractal_tree_tests "${TEST_SOURCES}")

# include libs
include_directories(${STXXL_INCLUDE_DIRS})
include_directories(${gtest_SOURCE_DIR} ${gtest_SOURCE_DIR}/include)
include_directories(${gmock_SOURCE_DIR} ${gmock_SOURCE_DIR}/include)

# link
target_link_libraries(fractal_tree_tests gtest gtest_main gmock gmock_main ${STXXL_LIBRARIES})
add_test(FractalTree fractal_tree_tests)