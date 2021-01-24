/*
 * run-fractal-tree.cpp
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran     <hung@ae.cs.uni-frankfurt.de>
 */

#include <foxxll/mng/block_alloc_strategy.hpp>
#include "include/fractal_tree/fractal_tree.h"


int main()
{
    using key_type = unsigned int;
    using data_type = unsigned int;
    using value_type = std::pair<key_type, data_type>;
    constexpr unsigned block_size = 2u << 12u; // 4kB blocks
    constexpr unsigned cache_size = 2u << 15u; // 32kB cache

    using ftree_type = stxxl::ftree<key_type, data_type, block_size, cache_size>;
    ftree_type f;

    // insert 1MB of data
    for (key_type k = 0; k < (2u << 20u) / sizeof(value_type); k++) {
        f.insert(value_type(k, 2*k));
    }

    // find datum of given key
    std::pair<data_type, bool> datum_and_found = f.find(1);
    assert(datum_and_found.second);
    assert(datum_and_found.first == 2);

    // find values in key range [100, 1000]
    std::vector<value_type> range_values = f.range_find(100, 1000);
    std::vector<value_type> correct_range_values {};
    for (key_type k = 100; k <= 1000; k++) {
        correct_range_values.emplace_back(k, 2*k);
    }
    assert(range_values == correct_range_values);


    return 0;
}