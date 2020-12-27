/*
 * run-fractal-tree.cpp
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran     <hung@ae.cs.uni-frankfurt.de>
 */

#include <tlx/die.hpp>
#include "include/fractal_tree/fractal_tree.h"
#include "include/fractal_tree/fractal_tree_cache.h"

using key_type = int;
using data_type = int;
using value_type = std::pair<key_type, data_type>;

constexpr unsigned RawBlockSize = 12;

using bid_type = foxxll::BID<RawBlockSize>;


constexpr unsigned num_items = RawBlockSize / sizeof(value_type);
/* TODO problem: need sizeof(typed_block) (which is == sizeof(block)) to be == raw_size.
Options: (a) add padding ourselves: char[RawSize-sizeof(block)] -> could again be padded?!
         (b) add metadata that fills the rest
         (c) custom serialization & deserialization

(c) not sure how we'd pack the data into e.g. a char array without lots of effort (could however check how FOXXLL does that)
(b) that does not seem to work: see run-test with n=1000 and a metadata struct for 96 bytes -> valgrind gives warning
(a) similar to (b); also does not seem to work: see run-test with leaf_block vs unpadded_leaf_block
    -> however, everything seems to work. Search for "valgrind" in aligned_alloc.hpp; Test with typed_block<e.g. int> and e.g. RawBlockSize=4095; read docs about that, maybe it's explained there.
        ; understand why even with exact size it does not seem to work!!!
*/
struct block {
    std::array<value_type, num_items> A;
    //std::array<char, RawBlockSize - num_items * sizeof(value_type)> padding{};
};

using block_type = foxxll::typed_block<RawBlockSize, block, 0, char>;
foxxll::block_manager* bm = foxxll::block_manager::get_instance();

struct bid_hash {
    size_t operator () (const bid_type& bid) const {
        size_t result = foxxll::longhash1(bid.offset + reinterpret_cast<uint64_t>(bid.storage));
        return result;
    }
};

void test_cache_basic() {
    constexpr unsigned num_blocks_in_cache = 2;
    using cache_type = fractal_tree_cache<block_type, bid_type, bid_hash, num_blocks_in_cache>;

    std::unordered_set<bid_type, bid_hash> dirty_bids;
    cache_type cache = cache_type(dirty_bids);

    assert_equal(num_blocks_in_cache, cache.num_cached_blocks() + cache.num_unused_blocks());
    assert_equal(cache.num_cached_blocks(), 0);
}

void test_cache_load_and_kick() {
    constexpr unsigned num_blocks_in_cache = 1;
    using cache_type = fractal_tree_cache<block_type, bid_type, bid_hash, num_blocks_in_cache>;

    std::unordered_set<bid_type, bid_hash> dirty_bids;
    cache_type cache = cache_type(dirty_bids);

    std::array<value_type, num_items>* data = nullptr;
    bid_type bid = bid_type();
    bm->new_block(foxxll::default_alloc_strategy(), bid);

    assert(!cache.is_cached(bid));
    assert(!cache.is_dirty(bid));

    // Load; write data to internal memory.
    block_type* block_for_data = cache.load(bid);
    data = &(block_for_data->begin()->A);
    for (size_t i=0; i<num_items; i++) {
        data->at(i) = value_type(i, i);
    }
    dirty_bids.insert(bid);

    assert(cache.is_cached(bid));
    assert(cache.is_dirty(bid));
    assert_equal(cache.num_cached_blocks(), 1);
    assert_equal(cache.num_unused_blocks(), 0);

    // Kick from cache
    cache.kick(bid);

    assert(!cache.is_cached(bid));
    assert(!cache.is_dirty(bid));
    assert_equal(cache.num_cached_blocks(), 0);
    assert_equal(cache.num_unused_blocks(), 1);

    // Load again.
    block_for_data = cache.load(bid);
    data = &(block_for_data->begin()->A);
    for (size_t i=0; i<num_items; i++) {
        assert(data->at(i) == value_type(i, i));
    }

    assert(cache.is_cached(bid));
    assert(!cache.is_dirty(bid));
    assert_equal(cache.num_cached_blocks(), 1);
    assert_equal(cache.num_unused_blocks(), 0);
}

void test_cache_dirty() {
    // Do not set dirty -> check if data is really overwritten
    // by second block that uses the cache. Assert that the blocks
    // are the same.
}

void test_cache_evict() {
    constexpr unsigned num_blocks_in_cache = 1;
    using cache_type = fractal_tree_cache<block_type, bid_type, bid_hash, num_blocks_in_cache>;

    std::unordered_set<bid_type, bid_hash> dirty_bids;
    cache_type cache = cache_type(dirty_bids);


    block *data1, *data2, *data3;
    bid_type bid1, bid2, bid3;
    bm->new_block(foxxll::default_alloc_strategy(), bid1);
    bm->new_block(foxxll::default_alloc_strategy(), bid2);
    bm->new_block(foxxll::default_alloc_strategy(), bid3);

    // Load data1, write to it
    block_type* block_for_data_1 = cache.load(bid1);
    data1 = block_for_data_1->begin();
    for (size_t i=0; i<num_items; i++) {
        data1->A[i] = value_type(i, i);
    }
    assert(cache.is_cached(bid1));


    for (size_t i=0; i<num_items; i++) {
        data1->A[i] = value_type(i, i);
        data2->A[i] = value_type(2*i, 2*i);
        data3->A[i] = value_type(3*i, 3*i);
    }

}


int main()
{

    test_cache_basic();
    test_cache_load_and_kick();

    return 0;
}