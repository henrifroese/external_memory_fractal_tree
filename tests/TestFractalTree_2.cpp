//
// Created by henri on 01.01.21.
//
#include <gtest/gtest.h>
#include "../include/fractal_tree/fractal_tree.h"

#include <algorithm>
#include <random>

using key_type = int;
using data_type = int;
using value_type = std::pair<key_type, data_type>;

constexpr unsigned RawBlockSize = 512;
constexpr unsigned RawMemoryPoolSize = 4096;

using ftree_type = stxxl::ftree<key_type, data_type, RawBlockSize, RawMemoryPoolSize>;

class TestFractalTree_2 : public ::testing::Test { };


TEST_F(TestFractalTree_2, test_fractal_tree_insert_fill_up_root_2) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_root_insertions_without_split = f.max_num_buffer_items_in_node;

    std::vector<value_type> to_insert {};
    for (int i=0; i<max_num_root_insertions_without_split; i++)
        to_insert.emplace_back(i, 2*i);
    auto rng = std::default_random_engine { 42 };
    std::shuffle(std::begin(to_insert), std::end(to_insert), rng);


    for (int i=0; i<max_num_root_insertions_without_split; i++) {
        f.insert(to_insert[i]);
        // Check inserted values are found
        for (int j=0; j<=i; j++) {
            ASSERT_TRUE(f.find(to_insert[i].first).second);
            ASSERT_EQ(f.find(to_insert[i].first).first, to_insert[i].second);
        }
        // Check not yet inserted values are not found
        for (int j=i+1; j<max_num_root_insertions_without_split; j++)
            ASSERT_FALSE(f.find(to_insert[j].first).second);
    }

    ASSERT_EQ(f.depth(), 1);
    ASSERT_EQ(f.num_leaves(), 0);
    ASSERT_EQ(f.num_nodes(), 1);
}

TEST_F(TestFractalTree_2, test_fractal_tree_insert_fill_up_root_and_duplicates_2) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_root_insertions_without_split = f.max_num_buffer_items_in_node;

    std::vector<value_type> to_insert {};
    for (int i=0; i<max_num_root_insertions_without_split; i++)
        to_insert.emplace_back(i, 2*i);
    auto rng = std::default_random_engine { 42 };
    std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

    for (int i=0; i<max_num_root_insertions_without_split; i++)
        f.insert(to_insert[i]);

    // Re-insert same keys with new data
    for (int i=0; i<max_num_root_insertions_without_split; i++) {
        // Insert with new data
        f.insert(value_type(to_insert[i].first, to_insert[i].second + 1));
        // Check overwritten values are updated
        for (int j=0; j<=i; j++) {
            ASSERT_TRUE(f.find(to_insert[j].first).second);
            ASSERT_EQ(f.find(to_insert[j].first).first, to_insert[j].second + 1);
        }
        // Check not-yet-overwritten values are found
        for (int j=i+1; j<max_num_root_insertions_without_split; j++) {
            ASSERT_TRUE(f.find(to_insert[j].first).second);
            ASSERT_EQ(f.find(to_insert[j].first).first, to_insert[j].second);
        }
    }
}

TEST_F(TestFractalTree_2, test_fractal_tree_insert_split_singular_root_2) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_root_insertions_without_split = f.max_num_buffer_items_in_node;

    std::vector<value_type> to_insert {};
    for (int i=0; i<max_num_root_insertions_without_split; i++)
        to_insert.emplace_back(i, 2*i);
    auto rng = std::default_random_engine { 42 };
    std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

    for (int i=0; i<max_num_root_insertions_without_split; i++)
        f.insert(to_insert[i]);
    // Root buffer is now full -> next insertion leads to split.
    f.insert(value_type(max_num_root_insertions_without_split, 2*max_num_root_insertions_without_split));
    // Fill up root buffer again
    for (int i=max_num_root_insertions_without_split+1; i<2*max_num_root_insertions_without_split; i++)
        f.insert(to_insert[i]);

    ASSERT_EQ(f.depth(), 2);
    ASSERT_EQ(f.num_leaves(), 2);
    ASSERT_EQ(f.num_nodes(), 1);
}


TEST_F(TestFractalTree_2, test_fractal_tree_insert_flush_bottom_buffer_2) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_root_insertions_without_split = f.max_num_buffer_items_in_node;

    std::vector<value_type> to_insert {};
    for (int i=0; i<3*max_num_root_insertions_without_split; i++)
        to_insert.emplace_back(i, 2*i);
    auto rng = std::default_random_engine { 42 };
    std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

    // Fill up root buffer twice (-> root has already split once now
    // and its buffer is again full).
    for (int i=0; i<2*max_num_root_insertions_without_split; i++)
        f.insert(to_insert[i]);

    // Root buffer is now full, but root values are not at least half full
    // -> will flush_bottom_buffer(root)
    f.insert(to_insert[2*max_num_root_insertions_without_split]);

    // Fill up root buffer again
    for (int i=2*max_num_root_insertions_without_split+1; i<3*max_num_root_insertions_without_split; i++)
        f.insert(to_insert[i]);

    // Check inserted values are found
    for (int i=0; i<3*max_num_root_insertions_without_split; i++) {
        ASSERT_TRUE(f.find(to_insert[i].first).second);
        ASSERT_EQ(f.find(to_insert[i].first).first, to_insert[i].second);
    }

    ASSERT_EQ(f.depth(), 2);
    ASSERT_EQ(f.num_leaves(), 2);
    ASSERT_EQ(f.num_nodes(), 1);
}


TEST_F(TestFractalTree_2, test_fractal_tree_insert_split_root_2) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_values = f.max_num_values_in_node;
    int max_num_values_until_root_half_full = (max_num_values - 1)/2;
    int max_num_root_insertions_until_root_values_half_full =
            f.max_num_buffer_items_in_node * (1 + max_num_values_until_root_half_full);

    std::vector<value_type> to_insert {};
    for (int i=0; i<=max_num_root_insertions_until_root_values_half_full; i++)
        to_insert.emplace_back(i, 2*i);
    auto rng = std::default_random_engine { 42 };
    std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

    // Fill up root buffer until it is half full
    for (int i=0; i<max_num_root_insertions_until_root_values_half_full; i++)
        f.insert(to_insert[i]);

    // Insertion that splits the root
    f.insert(to_insert[max_num_root_insertions_until_root_values_half_full]);

    // Check inserted values are found
    for (int i=0; i<=max_num_root_insertions_until_root_values_half_full; i++) {
        ASSERT_TRUE(f.find(to_insert[i].first).second);
        ASSERT_EQ(f.find(to_insert[i].first).first, to_insert[i].second);
    }

    ASSERT_EQ(f.depth(), 3);
    ASSERT_EQ(f.num_leaves(), 1 + max_num_values_until_root_half_full);
    ASSERT_EQ(f.num_nodes(), 3);
}


TEST_F(TestFractalTree_2, test_fractal_tree_insert_flush_buffer_2) {
    stxxl::ftree<int, int, 512, 4096> f;
    int max_num_values = f.max_num_values_in_node;
    int max_num_values_until_root_half_full = (max_num_values - 1)/2;
    int max_num_root_insertions_until_root_values_half_full =
            f.max_num_buffer_items_in_node * (1 + max_num_values_until_root_half_full);
    int max_num_insertions_until_depth_3_root_flush =
            f.max_num_buffer_items_in_node * (2 + max_num_values_until_root_half_full);

    std::vector<value_type> to_insert {};
    for (int i=0; i<=max_num_insertions_until_depth_3_root_flush; i++)
        to_insert.emplace_back(i, 2*i);
    auto rng = std::default_random_engine { 42 };
    std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

    // Fill up root buffer to get depth three
    for (int i=0; i<max_num_insertions_until_depth_3_root_flush; i++)
        f.insert(to_insert[i]);

    // Insertion that flushes the root
    f.insert(to_insert[max_num_insertions_until_depth_3_root_flush]);

    // Check inserted values are found
    for (int i=0; i<=max_num_insertions_until_depth_3_root_flush; i++) {
        ASSERT_TRUE(f.find(to_insert[i].first).second);
        ASSERT_EQ(f.find(to_insert[i].first).first, to_insert[i].second);
    }

    f.visualize();
    ASSERT_EQ(f.depth(), 3);
    ASSERT_EQ(f.num_leaves(), 4);
    ASSERT_EQ(f.num_nodes(), 3);

    std::cout << f.depth() << "     " << f.num_nodes() << "     " << f.num_leaves() << std::endl;

}


TEST_F(TestFractalTree_2, test_fractal_tree_visualize_2) {
    stxxl::ftree<int, int, 4096, 8*4096> f;

    std::vector<value_type> to_insert {};
    for (int i=0; i<=50000; i++)
        to_insert.emplace_back(i, 2*i);
    auto rng = std::default_random_engine { 42 };
    std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

    for (int i=0; i<50000; i++) {
        if ((i % 2000 == 0) && (i>0)) {
            f.visualize();
        }

        if (i == 16428)
            f.visualize();

        f.insert(to_insert[i]);
    }

    f.visualize();

    // Check inserted values are found
    for (int i=0; i<50000; i++) {
        ASSERT_TRUE(f.find(to_insert[i].first).second);
        ASSERT_EQ(f.find(to_insert[i].first).first, to_insert[i].second);
    }
}

/*
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
*/