/*
 * TestCache.cpp
 *
 * Copyright (C) 2020 Hung Tran <hung@ae.cs.uni-frankfurt.de>
 */

#include <gtest/gtest.h>
#include "../include/fractal_tree/fractal_tree.h"

using key_type = int;
using data_type = int;
using value_type = std::pair<key_type, data_type>;

constexpr unsigned RawBlockSize = 4096;

using bid_type = foxxll::BID<RawBlockSize>;

constexpr unsigned num_items = RawBlockSize / sizeof(value_type);

struct block {
    std::array<value_type, num_items> A {};
    std::array<char, RawBlockSize - num_items * sizeof(value_type)> padding {};
};

using block_type = foxxll::typed_block<RawBlockSize, block>;
foxxll::block_manager* bm;

struct bid_hash {
    size_t operator () (const bid_type& bid) const {
        size_t result = foxxll::longhash1(bid.offset + reinterpret_cast<uint64_t>(bid.storage));
        return result;
    }
};

template<typename BlockType, typename BidType, typename BidHash, unsigned NumBlocksInCache>
using fractal_tree_cache = stxxl::fractal_tree::fractal_tree_cache<BlockType, BidType, BidHash, NumBlocksInCache>;

class TestCache : public ::testing::Test { };

class foxxll_timer {
private:
    const std::string label;
    foxxll::stats* stats;
    const foxxll::stats_data stats_begin;
public:
    explicit foxxll_timer(std::string label_) : label(std::move(label_)), stats(foxxll::stats::get_instance()), stats_begin(*stats) { };
    ~foxxll_timer() { std::cout << label << ": " << std::endl << (foxxll::stats_data(*stats) - stats_begin) << std::endl; }
};

TEST_F(TestCache, test_cache_basic) {
foxxll_timer custom_timer("test_cache_basic");
constexpr unsigned num_blocks_in_cache = 2;
using cache_type = fractal_tree_cache<block_type, bid_type, bid_hash, num_blocks_in_cache>;

std::unordered_set<bid_type, bid_hash> dirty_bids;
cache_type cache = cache_type(dirty_bids);

ASSERT_EQ(num_blocks_in_cache, cache.num_cached_blocks() + cache.num_unused_blocks());
ASSERT_EQ(cache.num_cached_blocks(), 0);
}

TEST_F(TestCache, test_cache_load) {
std::array<value_type, num_items> data1;
data1.fill(value_type(1, 1));

bm = foxxll::block_manager::get_instance();
constexpr unsigned num_blocks_in_cache = 1;
using cache_type = fractal_tree_cache<block_type, bid_type, bid_hash, num_blocks_in_cache>;

std::unordered_set<bid_type, bid_hash> dirty_bids;
cache_type cache = cache_type(dirty_bids);

std::array<value_type, num_items>* data = nullptr;
bid_type bid = bid_type();
bm->new_block(foxxll::default_alloc_strategy(), bid);

ASSERT_FALSE(cache.is_cached(bid));
ASSERT_FALSE(cache.is_dirty(bid));

// Load; write data to internal memory.
block_type* block_for_data = cache.load(bid);
data = &(block_for_data->begin()->A);
*data = data1;
dirty_bids.insert(bid);

ASSERT_TRUE(cache.is_cached(bid));
ASSERT_TRUE(cache.is_dirty(bid));
ASSERT_EQ(cache.num_cached_blocks(), 1);
ASSERT_EQ(cache.num_unused_blocks(), 0);
}

TEST_F(TestCache, test_cache_load_kick) {
std::array<value_type, num_items> data1;
data1.fill(value_type(1, 1));

bm = foxxll::block_manager::get_instance();
constexpr unsigned num_blocks_in_cache = 1;
using cache_type = fractal_tree_cache<block_type, bid_type, bid_hash, num_blocks_in_cache>;

std::unordered_set<bid_type, bid_hash> dirty_bids;
cache_type cache = cache_type(dirty_bids);

std::array<value_type, num_items>* data = nullptr;
bid_type bid = bid_type();
bm->new_block(foxxll::default_alloc_strategy(), bid);

ASSERT_FALSE(cache.is_cached(bid));
ASSERT_FALSE(cache.is_dirty(bid));

// Load; write data to internal memory.
block_type* block_for_data = cache.load(bid);
data = &(block_for_data->begin()->A);
*data = data1;
dirty_bids.insert(bid);

ASSERT_TRUE(cache.is_cached(bid));
ASSERT_TRUE(cache.is_dirty(bid));
ASSERT_EQ(cache.num_cached_blocks(), 1);
ASSERT_EQ(cache.num_unused_blocks(), 0);

// Kick from cache
cache.kick(bid);

ASSERT_FALSE(cache.is_cached(bid));
ASSERT_FALSE(cache.is_dirty(bid));
ASSERT_EQ(cache.num_cached_blocks(), 0);
ASSERT_EQ(cache.num_unused_blocks(), 1);
}

TEST_F(TestCache, test_cache_dirty) {
// Do not set dirty in cache that holds 1 block
// -> check if data is really overwritten
// by second block that uses the cache. Assert that the blocks
// are the same.
std::array<value_type, num_items> data1;
data1.fill(value_type(1, 1));

std::array<value_type, num_items> data2;
data2.fill(value_type(2, 2));

bm = foxxll::block_manager::get_instance();
constexpr unsigned num_blocks_in_cache = 1;
using cache_type = fractal_tree_cache<block_type, bid_type, bid_hash, num_blocks_in_cache>;

std::unordered_set<bid_type, bid_hash> dirty_bids;
cache_type cache = cache_type(dirty_bids);

std::array<value_type, num_items>* data = nullptr;
bid_type bid1 = bid_type();
bm->new_block(foxxll::default_alloc_strategy(), bid1);
bid_type bid2 = bid_type();
bm->new_block(foxxll::default_alloc_strategy(), bid2);

// Load block; write data 1 to internal memory.
// Do not set bid1 dirty!
block_type* block_for_data1 = cache.load(bid1);
data = &(block_for_data1->begin()->A);
*data = data1;

ASSERT_TRUE(cache.is_cached(bid1));
ASSERT_FALSE(cache.is_cached(bid2));
ASSERT_EQ(*data, data1);
ASSERT_FALSE(cache.is_dirty(bid1));

// Load block; write data 2 to internal memory.
block_type* block_for_data2 = cache.load(bid2);
data = &(block_for_data2->begin()->A);
*data = data2;
dirty_bids.insert(bid2);

ASSERT_FALSE(cache.is_cached(bid1));
ASSERT_TRUE(cache.is_cached(bid2));
ASSERT_TRUE(cache.is_dirty(bid2));
ASSERT_EQ(*data, data2);
ASSERT_EQ(block_for_data1, block_for_data2);

// Load data 1 again.
ASSERT_EQ(cache.num_unused_blocks(), 0);
block_for_data1 = cache.load(bid1);
data = &(block_for_data1->begin()->A);
// Check that it is not equal to data 1,
// as data 1 was not written to disk, but
// rather equal to a default-initialized block.
ASSERT_NE(*data, data1);
std::array<value_type, num_items> data_default {};
ASSERT_EQ(*data, data_default);

ASSERT_TRUE(cache.is_cached(bid1));
ASSERT_FALSE(cache.is_cached(bid2));
ASSERT_EQ(cache.num_cached_blocks(), 1);
ASSERT_EQ(cache.num_unused_blocks(), 0);
}

TEST_F(TestCache, test_cache_evict) {
std::array<value_type, num_items> data1;
data1.fill(value_type(1, 1));

std::array<value_type, num_items> data2;
data2.fill(value_type(2, 2));

std::array<value_type, num_items> data3;
data3.fill(value_type(3, 3));

bm = foxxll::block_manager::get_instance();
constexpr unsigned num_blocks_in_cache = 2;
using cache_type = fractal_tree_cache<block_type, bid_type, bid_hash, num_blocks_in_cache>;

std::unordered_set<bid_type, bid_hash> dirty_bids;
cache_type cache = cache_type(dirty_bids);

std::array<value_type, num_items>* data = nullptr;
bid_type bid1 = bid_type();
bm->new_block(foxxll::default_alloc_strategy(), bid1);
bid_type bid2 = bid_type();
bm->new_block(foxxll::default_alloc_strategy(), bid2);
bid_type bid3 = bid_type();
bm->new_block(foxxll::default_alloc_strategy(), bid3);

// Load bid1
block_type* block_for_data1 = cache.load(bid1);
data = &(block_for_data1->begin()->A);
*data = data1;
dirty_bids.insert(bid1);

ASSERT_TRUE(cache.is_cached(bid1));
ASSERT_FALSE(cache.is_cached(bid2));
ASSERT_FALSE(cache.is_cached(bid3));
ASSERT_EQ(cache.num_cached_blocks(), 1);
ASSERT_EQ(cache.num_unused_blocks(), 1);
ASSERT_EQ(*data, data1);

// Load bid1
block_type* block_for_data2 = cache.load(bid2);
data = &(block_for_data2->begin()->A);
*data = data2;
dirty_bids.insert(bid2);

ASSERT_TRUE(cache.is_cached(bid1));
ASSERT_TRUE(cache.is_cached(bid2));
ASSERT_FALSE(cache.is_cached(bid3));
ASSERT_EQ(cache.num_cached_blocks(), 2);
ASSERT_EQ(cache.num_unused_blocks(), 0);
ASSERT_EQ(*data, data2);

// Load bid3 and check that the least recently
// used block (block 1) is evicted.
block_type* block_for_data3 = cache.load(bid3);
data = &(block_for_data3->begin()->A);
*data = data3;
dirty_bids.insert(bid3);

ASSERT_FALSE(cache.is_cached(bid1));
ASSERT_TRUE(cache.is_cached(bid2));
ASSERT_TRUE(cache.is_cached(bid3));
// 1 was kicked for 3 -> 3 reuses the block 1 used.
ASSERT_EQ(block_for_data1, block_for_data3);
ASSERT_EQ(cache.num_cached_blocks(), 2);
ASSERT_EQ(cache.num_unused_blocks(), 0);
ASSERT_EQ(*data, data3);

// Load bid1 and check that the least recently
// used block (block 2) is evicted.
block_for_data1 = cache.load(bid1);
data = &(block_for_data1->begin()->A);
ASSERT_EQ(*data, data1);

ASSERT_TRUE(cache.is_cached(bid1));
ASSERT_FALSE(cache.is_cached(bid2));
ASSERT_TRUE(cache.is_cached(bid3));
// 2 was kicked for 1 -> 1 reuses the block 2 used.
ASSERT_EQ(block_for_data1, block_for_data2);
ASSERT_EQ(cache.num_cached_blocks(), 2);
ASSERT_EQ(cache.num_unused_blocks(), 0);
}

