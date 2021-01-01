//
// Created by henri on 31.12.20.
//

#include <gtest/gtest.h>
#include "../include/fractal_tree/node.h"
#include "../include/fractal_tree/fractal_tree_cache.h"
#include "../include/fractal_tree/fractal_tree.h"

using key_type = int;
using data_type = int;
using value_type = std::pair<key_type, data_type>;

constexpr unsigned RawBlockSize = 4096;

using bid_type = foxxll::BID<RawBlockSize>;

constexpr unsigned num_items = RawBlockSize / sizeof(value_type);


struct bid_hash {
    size_t operator () (const bid_type& bid) const {
        size_t result = foxxll::longhash1(bid.offset + reinterpret_cast<uint64_t>(bid.storage));
        return result;
    }
};

template<typename BlockType, typename BidType, typename BidHash, unsigned NumBlocksInCache>
using fractal_tree_cache = stxxl::fractal_tree::fractal_tree_cache<BlockType, BidType, BidHash, NumBlocksInCache>;

using node_type = stxxl::fractal_tree::node<int, int, RawBlockSize>;
using leaf_type = stxxl::fractal_tree::leaf<int, int, RawBlockSize>;

class TestCacheWithNode : public ::testing::Test { };

TEST_F(TestCacheWithNode, test_cache_with_node_load) {
    foxxll::block_manager* bm = foxxll::block_manager::get_instance();
    constexpr unsigned num_blocks_in_cache = 1;
    using cache_type = fractal_tree_cache<node_type::block_type, bid_type, bid_hash, num_blocks_in_cache>;
    std::unordered_set<bid_type, bid_hash> dirty_bids;

    node_type n(0, bid_type());
    cache_type cache = cache_type(dirty_bids);

    bm->new_block(foxxll::default_alloc_strategy(), n.get_bid());

    ASSERT_FALSE(cache.is_cached(n.get_bid()));
    ASSERT_FALSE(cache.is_dirty(n.get_bid()));

// Load; write data to internal memory.
    node_type::block_type* cached_block = cache.load(n.get_bid());
    n.set_block(cached_block);
    std::vector<value_type> buffer_items { {0,0} };
    n.set_buffer(buffer_items);
    dirty_bids.insert(n.get_bid());

    ASSERT_EQ(n.num_items_in_buffer(), 1);
    ASSERT_EQ(n.num_values(), 0);
    ASSERT_TRUE(n.get_values().empty());
    ASSERT_TRUE(cache.is_cached(n.get_bid()));
    ASSERT_TRUE(cache.is_dirty(n.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 1);
    ASSERT_EQ(cache.num_unused_blocks(), 0);
}

TEST_F(TestCacheWithNode, test_cache_with_node_evict) {
    // Test loading+storing+evicting with nodes.
    std::vector<value_type> buffer1 { {1,1} };
    std::vector<value_type> buffer2 { {2,2} };
    std::vector<value_type> buffer3 { {3,3} };
    std::vector<value_type> b1 = buffer1;
    std::vector<value_type> b2 = buffer2;
    std::vector<value_type> b3 = buffer3;

    std::vector<value_type> values1 { {4,1} };
    std::vector<value_type> values2 { {5,2} };
    std::vector<value_type> values3 { {6,3} };
    std::vector<value_type> v1 = values1;
    std::vector<value_type> v2 = values2;
    std::vector<value_type> v3 = values3;

    std::vector<int> nodeIDs1 { 7, 1 };
    std::vector<int> nodeIDs2 { 8, 2 };
    std::vector<int> nodeIDs3 { 9, 3 };
    std::vector<int> no1 = nodeIDs1;
    std::vector<int> no2 = nodeIDs2;
    std::vector<int> no3 = nodeIDs3;

    foxxll::block_manager* bm = foxxll::block_manager::get_instance();
    constexpr unsigned num_blocks_in_cache = 2;
    using cache_type = fractal_tree_cache<node_type::block_type, bid_type, bid_hash, num_blocks_in_cache>;
    std::unordered_set<bid_type, bid_hash> dirty_bids;

    node_type n1(1, bid_type());
    node_type n2(2, bid_type());
    node_type n3(3, bid_type());
    cache_type cache = cache_type(dirty_bids);

    bm->new_block(foxxll::default_alloc_strategy(), n1.get_bid());
    bm->new_block(foxxll::default_alloc_strategy(), n2.get_bid());
    bm->new_block(foxxll::default_alloc_strategy(), n3.get_bid());

    // Load node1
    node_type::block_type* block_for_n1 = cache.load(n1.get_bid());
    n1.set_block(block_for_n1);
    n1.set_values_and_nodeIDs(values1, nodeIDs1);
    n1.set_buffer(buffer1);
    dirty_bids.insert(n1.get_bid());

    ASSERT_TRUE(cache.is_cached(n1.get_bid()));
    ASSERT_FALSE(cache.is_cached(n2.get_bid()));
    ASSERT_FALSE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 1);
    ASSERT_EQ(cache.num_unused_blocks(), 1);
    
    ASSERT_EQ(n1.get_values(), v1);
    ASSERT_EQ(n1.get_buffer_items(), b1);
    ASSERT_EQ(n1.get_nodeIDs(0, n1.num_children()), no1);

    // Load node2
    node_type::block_type* block_for_n2 = cache.load(n2.get_bid());
    n2.set_block(block_for_n2);
    n2.set_values_and_nodeIDs(values2, nodeIDs2);
    n2.set_buffer(buffer2);
    dirty_bids.insert(n2.get_bid());

    ASSERT_TRUE(cache.is_cached(n1.get_bid()));
    ASSERT_TRUE(cache.is_cached(n2.get_bid()));
    ASSERT_FALSE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 2);
    ASSERT_EQ(cache.num_unused_blocks(), 0);
    
    ASSERT_EQ(n1.get_values(), v1);
    ASSERT_EQ(n1.get_buffer_items(), b1);
    ASSERT_EQ(n1.get_nodeIDs(0, n1.num_children()), no1);
    ASSERT_EQ(n2.get_values(), v2);
    ASSERT_EQ(n2.get_buffer_items(), b2);
    ASSERT_EQ(n2.get_nodeIDs(0, n2.num_children()), no2);

    // Load node3 and check that the least recently used node
    // (node1) is evicted.
    node_type::block_type* block_for_n3 = cache.load(n3.get_bid());
    n3.set_block(block_for_n3);
    n3.set_values_and_nodeIDs(values3, nodeIDs3);
    n3.set_buffer(buffer3);
    dirty_bids.insert(n3.get_bid());

    ASSERT_FALSE(cache.is_cached(n1.get_bid()));
    ASSERT_TRUE(cache.is_cached(n2.get_bid()));
    ASSERT_TRUE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 2);
    ASSERT_EQ(cache.num_unused_blocks(), 0);
    
    ASSERT_EQ(n2.get_values(), v2);
    ASSERT_EQ(n2.get_buffer_items(), b2);
    ASSERT_EQ(n2.get_nodeIDs(0, n2.num_children()), no2);
    ASSERT_EQ(n3.get_values(), v3);
    ASSERT_EQ(n3.get_buffer_items(), b3);
    ASSERT_EQ(n3.get_nodeIDs(0, n3.num_children()), no3);

    // 1 was kicked for 3 -> 3 reuses the block 1 used.
    ASSERT_EQ(block_for_n1, block_for_n3);

    // Load node1 and check that the least recently used node
    // (node2) is evicted. Check that loading succeeded.
    block_for_n1 = cache.load(n1.get_bid());
    n1.set_block(block_for_n1);

    ASSERT_TRUE(cache.is_cached(n1.get_bid()));
    ASSERT_FALSE(cache.is_cached(n2.get_bid()));
    ASSERT_TRUE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 2);
    ASSERT_EQ(cache.num_unused_blocks(), 0);

    ASSERT_EQ(n1.get_values(), v1);
    ASSERT_EQ(n1.get_buffer_items(), b1);
    ASSERT_EQ(n1.get_nodeIDs(0, n1.num_children()), no1);
    ASSERT_EQ(n3.get_values(), v3);
    ASSERT_EQ(n3.get_buffer_items(), b3);
    ASSERT_EQ(n3.get_nodeIDs(0, n3.num_children()), no3);

    // 2 was kicked for 1 -> 1 reuses the block 2 used.
    ASSERT_EQ(block_for_n1, block_for_n2);

    // Load node2 and check that the least recently used node
    // (node3) is evicted. Check that loading succeeded.
    block_for_n2 = cache.load(n2.get_bid());
    n2.set_block(block_for_n2);

    ASSERT_TRUE(cache.is_cached(n1.get_bid()));
    ASSERT_TRUE(cache.is_cached(n2.get_bid()));
    ASSERT_FALSE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 2);
    ASSERT_EQ(cache.num_unused_blocks(), 0);

    ASSERT_EQ(n1.get_values(), v1);
    ASSERT_EQ(n1.get_buffer_items(), b1);
    ASSERT_EQ(n1.get_nodeIDs(0, n1.num_children()), no1);
    ASSERT_EQ(n2.get_values(), v2);
    ASSERT_EQ(n2.get_buffer_items(), b2);
    ASSERT_EQ(n2.get_nodeIDs(0, n2.num_children()), no2);

    // 3 was kicked for 2 -> 2 reuses the block 3 used.
    ASSERT_EQ(block_for_n2, block_for_n3);

    // Load node3 and check that the least recently used node
    // (node1) is evicted. Check that loading succeeded.
    block_for_n3 = cache.load(n3.get_bid());
    n3.set_block(block_for_n3);

    ASSERT_FALSE(cache.is_cached(n1.get_bid()));
    ASSERT_TRUE(cache.is_cached(n2.get_bid()));
    ASSERT_TRUE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 2);
    ASSERT_EQ(cache.num_unused_blocks(), 0);

    ASSERT_EQ(n2.get_values(), v2);
    ASSERT_EQ(n2.get_buffer_items(), b2);
    ASSERT_EQ(n2.get_nodeIDs(0, n2.num_children()), no2);
    ASSERT_EQ(n3.get_values(), v3);
    ASSERT_EQ(n3.get_buffer_items(), b3);
    ASSERT_EQ(n3.get_nodeIDs(0, n3.num_children()), no3);

    // 1 was kicked for 3 -> 3 reuses the block 1 used.
    ASSERT_EQ(block_for_n1, block_for_n3);
}


TEST_F(TestCacheWithNode, test_cache_with_leaf_evict) {
    // Test loading+storing+evicting with nodes.
    std::vector<value_type> buffer1 { {1,1} };
    std::vector<value_type> buffer2 { {2,2} };
    std::vector<value_type> buffer3 { {3,3} };
    std::vector<value_type> b1 = buffer1;
    std::vector<value_type> b2 = buffer2;
    std::vector<value_type> b3 = buffer3;

    foxxll::block_manager* bm = foxxll::block_manager::get_instance();
    constexpr unsigned num_blocks_in_cache = 2;
    using cache_type = fractal_tree_cache<leaf_type::block_type, bid_type, bid_hash, num_blocks_in_cache>;
    std::unordered_set<bid_type, bid_hash> dirty_bids;

    leaf_type n1(1, bid_type());
    leaf_type n2(2, bid_type());
    leaf_type n3(3, bid_type());
    cache_type cache = cache_type(dirty_bids);

    bm->new_block(foxxll::default_alloc_strategy(), n1.get_bid());
    bm->new_block(foxxll::default_alloc_strategy(), n2.get_bid());
    bm->new_block(foxxll::default_alloc_strategy(), n3.get_bid());

    // Load leaf1
    leaf_type::block_type* block_for_n1 = cache.load(n1.get_bid());
    n1.set_block(block_for_n1);
    n1.set_buffer(buffer1);
    dirty_bids.insert(n1.get_bid());

    ASSERT_TRUE(cache.is_cached(n1.get_bid()));
    ASSERT_FALSE(cache.is_cached(n2.get_bid()));
    ASSERT_FALSE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 1);
    ASSERT_EQ(cache.num_unused_blocks(), 1);

    ASSERT_EQ(n1.get_buffer_items(), b1);

    // Load leaf2
    leaf_type::block_type* block_for_n2 = cache.load(n2.get_bid());
    n2.set_block(block_for_n2);
    n2.set_buffer(buffer2);
    dirty_bids.insert(n2.get_bid());

    ASSERT_TRUE(cache.is_cached(n1.get_bid()));
    ASSERT_TRUE(cache.is_cached(n2.get_bid()));
    ASSERT_FALSE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 2);
    ASSERT_EQ(cache.num_unused_blocks(), 0);

    ASSERT_EQ(n1.get_buffer_items(), b1);
    ASSERT_EQ(n2.get_buffer_items(), b2);

    // Load leaf3 and check that the least recently used leaf
    // (leaf1) is evicted.
    leaf_type::block_type* block_for_n3 = cache.load(n3.get_bid());
    n3.set_block(block_for_n3);
    n3.set_buffer(buffer3);
    dirty_bids.insert(n3.get_bid());

    ASSERT_FALSE(cache.is_cached(n1.get_bid()));
    ASSERT_TRUE(cache.is_cached(n2.get_bid()));
    ASSERT_TRUE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 2);
    ASSERT_EQ(cache.num_unused_blocks(), 0);

    ASSERT_EQ(n2.get_buffer_items(), b2);
    ASSERT_EQ(n3.get_buffer_items(), b3);

    // 1 was kicked for 3 -> 3 reuses the block 1 used.
    ASSERT_EQ(block_for_n1, block_for_n3);

    // Load leaf1 and check that the least recently used leaf
    // (leaf2) is evicted. Check that loading succeeded.
    block_for_n1 = cache.load(n1.get_bid());
    n1.set_block(block_for_n1);

    ASSERT_TRUE(cache.is_cached(n1.get_bid()));
    ASSERT_FALSE(cache.is_cached(n2.get_bid()));
    ASSERT_TRUE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 2);
    ASSERT_EQ(cache.num_unused_blocks(), 0);

    ASSERT_EQ(n1.get_buffer_items(), b1);
    ASSERT_EQ(n3.get_buffer_items(), b3);

    // 2 was kicked for 1 -> 1 reuses the block 2 used.
    ASSERT_EQ(block_for_n1, block_for_n2);

    // Load leaf2 and check that the least recently used leaf
    // (leaf3) is evicted. Check that loading succeeded.
    block_for_n2 = cache.load(n2.get_bid());
    n2.set_block(block_for_n2);

    ASSERT_TRUE(cache.is_cached(n1.get_bid()));
    ASSERT_TRUE(cache.is_cached(n2.get_bid()));
    ASSERT_FALSE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 2);
    ASSERT_EQ(cache.num_unused_blocks(), 0);

    ASSERT_EQ(n1.get_buffer_items(), b1);
    ASSERT_EQ(n2.get_buffer_items(), b2);

    // 3 was kicked for 2 -> 2 reuses the block 3 used.
    ASSERT_EQ(block_for_n2, block_for_n3);

    // Load leaf3 and check that the least recently used leaf
    // (leaf1) is evicted. Check that loading succeeded.
    block_for_n3 = cache.load(n3.get_bid());
    n3.set_block(block_for_n3);

    ASSERT_FALSE(cache.is_cached(n1.get_bid()));
    ASSERT_TRUE(cache.is_cached(n2.get_bid()));
    ASSERT_TRUE(cache.is_cached(n3.get_bid()));
    ASSERT_EQ(cache.num_cached_blocks(), 2);
    ASSERT_EQ(cache.num_unused_blocks(), 0);

    ASSERT_EQ(n2.get_buffer_items(), b2);
    ASSERT_EQ(n3.get_buffer_items(), b3);

    // 1 was kicked for 3 -> 3 reuses the block 1 used.
    ASSERT_EQ(block_for_n1, block_for_n3);
}
