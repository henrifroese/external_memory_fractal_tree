/*
 * run-fractal-tree.cpp
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran     <hung@ae.cs.uni-frankfurt.de>
 */

#include <foxxll/mng/block_alloc_strategy.hpp>
#include "include/fractal_tree/fractal_tree.h"

using key_type = int;
using data_type = int;
using value_type = std::pair<key_type, data_type>;

constexpr unsigned NumBlocksInCache = 2;
constexpr unsigned RawBlockSize = 10;

using bid_type = foxxll::BID<RawBlockSize>;

struct bid_hash {
    size_t operator () (const bid_type& bid) const {
        size_t result = foxxll::longhash1(bid.offset + reinterpret_cast<uint64_t>(bid.storage));
        return result;
    }
};

struct block {
    std::array<value_type, RawBlockSize / 2 / sizeof(value_type)> A;
    std::array<value_type, RawBlockSize / 2 / sizeof(value_type)> B;
};

using block_type = foxxll::typed_block<RawBlockSize, block>;

using ftree_type = stxxl::ftree<key_type, data_type, 512, 4096>;


void test_cache() {


}


int main()
{

    ftree_type f_tree;

    return 0;
}