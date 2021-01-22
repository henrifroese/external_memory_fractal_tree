//
// Created by henri on 01.01.21.
//
#include <gtest/gtest.h>
#include "../include/fractal_tree/fractal_tree.h"
#include <random>
#include <algorithm>

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
    stxxl::ftree<std::pair<double, char>, int, 2048, 16384> f7;
    stxxl::ftree<std::pair<double, double>, double, 4096, 32768> f8;
    stxxl::ftree<std::pair<double, double>, bool, 2048, 16384> f9;
    stxxl::ftree<std::array<double, 10>, std::array<double, 3>, 4096 * 4, 4096 * 32> f10;
}

TEST_F(TestFractalTree, test_fractal_tree_find_empty) {
    stxxl::ftree<int, int, 512, 4096> f;
    for (int i=-100; i<100; i++)
        ASSERT_FALSE(f.find(i).second);

    stxxl::ftree<double, std::array<char, 2>, 1024, 8192> f2;
    for (int i=-100; i<100; i++)
        ASSERT_FALSE(f.find((double) i).second);

    ASSERT_EQ(f.depth(), 1);
    ASSERT_EQ(f.num_leaves(), 0);
    ASSERT_EQ(f.num_nodes(), 1);
}

TEST_F(TestFractalTree, test_fractal_tree_insert_basic) {
    stxxl::ftree<int, int, 512, 4096> f;
    f.insert(value_type(0, 10));
    ASSERT_TRUE(f.find(0).second);
    ASSERT_EQ(f.find(0).first, 10);

    ASSERT_EQ(f.depth(), 1);
    ASSERT_EQ(f.num_leaves(), 0);
    ASSERT_EQ(f.num_nodes(), 1);
}

TEST_F(TestFractalTree, test_fractal_tree_insert_fill_up_root) {
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

    ASSERT_EQ(f.depth(), 1);
    ASSERT_EQ(f.num_leaves(), 0);
    ASSERT_EQ(f.num_nodes(), 1);
}

TEST_F(TestFractalTree, test_fractal_tree_insert_fill_up_root_and_duplicates) {
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

TEST_F(TestFractalTree, test_fractal_tree_insert_split_singular_root) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_root_insertions_without_split = f.max_num_buffer_items_in_node;
    for (int i=0; i<max_num_root_insertions_without_split; i++)
        f.insert(value_type(i, 2*i));
    // Root buffer is now full -> next insertion leads to split.
    f.insert(value_type(max_num_root_insertions_without_split, 2*max_num_root_insertions_without_split));
    // Fill up root buffer again
    for (int i=max_num_root_insertions_without_split+1; i<2*max_num_root_insertions_without_split; i++)
        f.insert(value_type(i, 2*i));

    ASSERT_EQ(f.depth(), 2);
    ASSERT_EQ(f.num_leaves(), 2);
    ASSERT_EQ(f.num_nodes(), 1);
}

TEST_F(TestFractalTree, test_fractal_tree_insert_flush_bottom_buffer) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_root_insertions_without_split = f.max_num_buffer_items_in_node;

    // Fill up root buffer twice (-> root has already split once now
    // and its buffer is again full).
    for (int i=0; i<2*max_num_root_insertions_without_split; i++)
        f.insert(value_type(i, 2*i));

    // Root buffer is now full, but root values are not at least half full
    // -> will flush_bottom_buffer(root)
    f.insert(value_type(2*max_num_root_insertions_without_split, 2*2*max_num_root_insertions_without_split));

    // Fill up root buffer again
    for (int i=2*max_num_root_insertions_without_split+1; i<3*max_num_root_insertions_without_split; i++)
        f.insert(value_type(i, 2*i));

    // Check inserted values are found
    for (int i=0; i<3*max_num_root_insertions_without_split; i++) {
        ASSERT_TRUE(f.find(i).second);
        ASSERT_EQ(f.find(i).first, 2*i);
    }

    ASSERT_EQ(f.depth(), 2);
    ASSERT_EQ(f.num_leaves(), 3);
    ASSERT_EQ(f.num_nodes(), 1);
}

TEST_F(TestFractalTree, test_fractal_tree_insert_split_root) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_values = f.max_num_values_in_node;
    int max_num_values_until_root_half_full = (max_num_values - 1)/2;
    int max_num_root_insertions_until_root_values_half_full =
            f.max_num_buffer_items_in_node * (1 + max_num_values_until_root_half_full);

    // Fill up root buffer until it is half full
    for (int i=0; i<max_num_root_insertions_until_root_values_half_full; i++)
        f.insert(value_type(i, 2*i));

    // Insertion that splits the root
    f.insert(value_type(max_num_root_insertions_until_root_values_half_full, 2*(max_num_root_insertions_until_root_values_half_full)));

    // Check inserted values are found
    for (int i=0; i<=max_num_root_insertions_until_root_values_half_full; i++) {
        ASSERT_TRUE(f.find(i).second);
        ASSERT_EQ(f.find(i).first, 2*i);
    }

    ASSERT_EQ(f.depth(), 3);
    ASSERT_EQ(f.num_leaves(), 1 + max_num_values_until_root_half_full);
    ASSERT_EQ(f.num_nodes(), 3);
}

TEST_F(TestFractalTree, test_fractal_tree_insert_flush_buffer) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_values = f.max_num_values_in_node;
    int max_num_values_until_root_half_full = (max_num_values - 1)/2;
    int max_num_root_insertions_until_root_values_half_full =
            f.max_num_buffer_items_in_node * (1 + max_num_values_until_root_half_full);
    int max_num_insertions_until_depth_3_root_flush =
            f.max_num_buffer_items_in_node * (2 + max_num_values_until_root_half_full);

    // Fill up root buffer to get depth three
    for (int i=0; i<max_num_insertions_until_depth_3_root_flush; i++)
        f.insert(value_type(i, 2*i));

    ASSERT_EQ(f.depth(), 3);

    // Insertion that flushes the root
    f.insert(value_type(max_num_insertions_until_depth_3_root_flush, 2*(max_num_insertions_until_depth_3_root_flush)));

    // Check inserted values are found
    for (int i=0; i<=max_num_insertions_until_depth_3_root_flush; i++) {
        ASSERT_TRUE(f.find(i).second);
        ASSERT_EQ(f.find(i).first, 2*i);
    }

    ASSERT_EQ(f.depth(), 3);
    ASSERT_EQ(f.num_leaves(), 2 + max_num_values_until_root_half_full);
    ASSERT_EQ(f.num_nodes(), 3);

    std::cout << f.depth() << "     " << f.num_nodes() << "     " << f.num_leaves() << std::endl;

}

TEST_F(TestFractalTree, test_fractal_tree_visualize) {
    stxxl::ftree<int, int, 4096, 8*4096> f;

    for (int i=0; i<10000; i++)
        f.insert(value_type(i, 2*i));

    f.visualize();

    // Check inserted values are found
    for (int i=0; i<10000; i++) {
        std::cout << i << std::endl;
        ASSERT_TRUE(f.find(i).second);
        ASSERT_EQ(f.find(i).first, 2*i);
    }
}

TEST_F(TestFractalTree, test_fractal_tree_insert_512kb) {
    stxxl::ftree<int, int, 4096, 8*4096> f;
    // Cache size is 4 kb;
    // one key-datum pair is 8 bytes.
    // Insert 512 kb.
    for (int i=0; i<512*1024/8; i++)
        f.insert(value_type(i, 2*i));

    std::cout << f.depth() << "     " << f.num_nodes() << "     " << f.num_leaves() << std::endl;

    // Check inserted values are found
    for (int i=0; i<512*1024/8; i++) {
        ASSERT_TRUE(f.find(i).second);
        ASSERT_EQ(f.find(i).first, 2*i);
    }
}

TEST_F(TestFractalTree, test_fractal_tree_insert_1mb) {
    stxxl::ftree<int, int, 4096, 8*4096> f;
    // Cache size is 4 kb;
    // one key-datum pair is 8 bytes.
    // Insert 1 mb.
    for (int i=0; i<1024*1024/8; i++)
        f.insert(value_type(i, 2*i));

    // Check inserted values are found
    for (int i=0; i<1024*1024/8; i++) {
        ASSERT_TRUE(f.find(i).second);
        ASSERT_EQ(f.find(i).first, 2*i);
    }
}

TEST_F(TestFractalTree, test_fractal_tree_insert_4mb) {
    stxxl::ftree<int, int, 4096, 2*1024*1024> f;
    // Cache size is 4 mb;
    // one key-datum pair is 8 bytes.
    // Insert 32 mb.
    for (int i=0; i<4*1024*1024/8; i++)
        f.insert(value_type(i, 2*i));

    std::cout << f.depth() << "     " << f.num_nodes() << "     " << f.num_leaves() << std::endl;

    // Check inserted values are found
    for (int i=0; i<4*1024*1024/8; i++) {
        ASSERT_TRUE(f.find(i).second);
        ASSERT_EQ(f.find(i).first, 2*i);
    }
}


TEST_F(TestFractalTree, test_fractal_tree_range_search) {
    stxxl::ftree<int, int, 4096, 2*1024*1024> f;

    int values_to_insert = 4*1024*1024/8;
    std::vector<value_type> to_insert {};
    to_insert.reserve(values_to_insert);
    for (int i=0; i<values_to_insert; i++)
        to_insert.emplace_back(i,i);

    auto rng = std::default_random_engine { 42 };
    std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

    for (auto val : to_insert)
        f.insert(val);

    std::vector<value_type> v = f.range_find(0, 100);
    std::vector<value_type> w {};
    for (int i=0; i<= 100; i++)
        w.emplace_back(i, i);
    ASSERT_TRUE(v.size() == w.size());
    for (int i=0; i<v.size(); i++) {
        ASSERT_TRUE(v[i] == w[i]);
    }

    v = f.range_find(0, 100000);
    w.clear();
    for (int i=0; i<= 100000; i++)
        w.emplace_back(i, i);
    ASSERT_TRUE(v.size() == w.size());
    for (int i=0; i<v.size(); i++) {
        ASSERT_TRUE(v[i] == w[i]);
    }

    v = f.range_find(100000, 10000001);
    w.clear();
    for (int i=std::min(100000, values_to_insert-1); i<=std::min(10000001, values_to_insert-1); i++)
        w.emplace_back(i, i);
    ASSERT_EQ(v.size(), w.size());
    for (int i=0; i<v.size(); i++) {
        ASSERT_TRUE(v[i] == w[i]);
    }

    v = f.range_find(123456, 524123);
    w.clear();
    for (int i=std::min(123456, values_to_insert-1); i<=std::min(524123, values_to_insert-1); i++)
        w.emplace_back(i, i);
    ASSERT_TRUE(v.size() == w.size());
    for (int i=0; i<v.size(); i++) {
        ASSERT_TRUE(v[i] == w[i]);
    }


    v = f.range_find(-100, -1);
    ASSERT_TRUE(v.empty());

    v = f.range_find(values_to_insert, values_to_insert + 100);
    ASSERT_TRUE(v.empty());

    v = f.range_find(-100, 0);
    ASSERT_TRUE(v.size() == 1);
    ASSERT_TRUE(v[0] == value_type(0,0));

}