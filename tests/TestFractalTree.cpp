//
// Created by henri on 01.01.21.
//
#include <gtest/gtest.h>
#include "../include/fractal_tree/fractal_tree.h"

using key_type = int;
using data_type = int;
using value_type = std::pair<key_type, data_type>;

constexpr unsigned RawBlockSize = 512;
constexpr unsigned RawMemoryPoolSize = 4096;

using ftree_type = stxxl::ftree<key_type, data_type, RawBlockSize, RawMemoryPoolSize>;

class TestFractalTree : public ::testing::Test { };

TEST_F(TestFractalTree, test_fractal_tree_parameters) {
    // This tests for the static assertions in the node class
    // for different key and datum types
    // (-> if this compiles, the tests are passing).
    stxxl::ftree<int, int, 512, 4096> f;
    stxxl::ftree<double, double, 1024, 8192> f2;
    stxxl::ftree<int, double, 1024, 8192> f3;
    stxxl::ftree<double, int, 1024, 8192> f4;
    stxxl::ftree<char, int, 512, 4096> f5;
    stxxl::ftree<std::pair<char, char>, int, 512, 4096> f6;
    stxxl::ftree<std::pair<double, char>, int, 1024, 8192> f7;
    stxxl::ftree<std::pair<double, double>, double, 1024, 8192> f8;
    stxxl::ftree<std::pair<double, double>, bool, 1024, 8192> f9;
    stxxl::ftree<std::array<double, 10>, std::array<double, 3>, 4096, 4096 * 8> f10;
}

TEST_F(TestFractalTree, test_fractal_tree_find_empty) {
    stxxl::ftree<int, int, 512, 4096> f;
    for (int i=-100; i<100; i++)
        ASSERT_FALSE(f.find(i).second);

    stxxl::ftree<double, std::array<char, 2>, 1024, 8192> f2;
    for (int i=-100; i<100; i++)
        ASSERT_FALSE(f.find((double) i).second);
}

TEST_F(TestFractalTree, test_fractal_tree_insert_root_only_basic) {
    stxxl::ftree<int, int, 512, 4096> f;
    f.insert(value_type(0, 10));
    ASSERT_TRUE(f.find(0).second);
    ASSERT_EQ(f.find(0).first, 10);
}

TEST_F(TestFractalTree, test_fractal_tree_insert_root_only) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_root_insertions_without_split = f.max_num_buffer_items_in_node;
    for (int i=0; i<max_num_root_insertions_without_split; i++) {
        f.insert(value_type(i, 2*i));
        // Check inserted values are found
        for (int j=0; j<=i; j++) {
            ASSERT_TRUE(f.find(j).second);
            ASSERT_EQ(f.find(j).first, 2*j);
        }
        // Check not yet inserted values are not found
        for (int j=i+1; j<max_num_root_insertions_without_split; j++)
            ASSERT_FALSE(f.find(j).second);
    }
}

TEST_F(TestFractalTree, test_fractal_tree_insert_root_only_duplicates) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_root_insertions_without_split = f.max_num_buffer_items_in_node;
    for (int i=0; i<max_num_root_insertions_without_split; i++)
        f.insert(value_type(i, 2*i));

    // Re-insert same keys with new data
    for (int i=0; i<max_num_root_insertions_without_split; i++) {
        // Insert with new data
        f.insert(value_type(i, 2*i+1));
        // Check overwritten values are updated
        for (int j=0; j<=i; j++) {
            ASSERT_TRUE(f.find(j).second);
            ASSERT_EQ(f.find(j).first, 2*j+1);
        }
        // Check not-yet-overwritten values are found
        for (int j=i+1; j<max_num_root_insertions_without_split; j++) {
            ASSERT_TRUE(f.find(j).second);
            ASSERT_EQ(f.find(j).first, 2*j);
        }
    }
}

TEST_F(TestFractalTree, test_fractal_tree_insert_root_split) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_root_insertions_without_split = f.max_num_buffer_items_in_node;
    for (int i=0; i<max_num_root_insertions_without_split; i++)
        f.insert(value_type(i, 2*i));
    // Root buffer is now full -> next insertion leads to split.
    f.insert(value_type(max_num_root_insertions_without_split, 2*max_num_root_insertions_without_split));
    // Fill up root buffer again
    for (int i=max_num_root_insertions_without_split+1; i<2*max_num_root_insertions_without_split; i++)
        f.insert(value_type(i, 2*i));
}

TEST_F(TestFractalTree, test_fractal_tree_insert_flush_root_buffer) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_root_insertions_without_split = f.max_num_buffer_items_in_node;

    // Fill up root buffer twice (-> root has already split once now
    // and its buffer is again full).
    for (int i=0; i<2*max_num_root_insertions_without_split; i++)
        f.insert(value_type(i, 2*i));

    // Root buffer is now full, but root values are not at least half full
    // -> will flush_bottom_buffer(root)
    /*
     * Situation:    28   [57..113]
     *          0..27   29..56
     */
    f.insert(value_type(2*max_num_root_insertions_without_split, 2*2*max_num_root_insertions_without_split));

    // Fill up root buffer again
    for (int i=2*max_num_root_insertions_without_split+1; i<3*max_num_root_insertions_without_split; i++)
        f.insert(value_type(i, 2*i));
}

