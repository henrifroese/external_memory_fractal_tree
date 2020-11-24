/*
 * run-fractal-tree.cpp
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran     <hung@ae.cs.uni-frankfurt.de>
 */

#include <iostream>
#include <tlx/unused.hpp>
#include <foxxll/mng/block_alloc_strategy.hpp>
#include <stxxl/comparator>
#include "include/fractal_tree/fractal_tree.h"

using key_type = int;
using payload_type = double;
using comp_type = stxxl::comparator<key_type>;
using pair = std::pair<key_type, payload_type>;
using ftree_type = stxxl::ftree<key_type, payload_type, comp_type, 4096, 4096, foxxll::simple_random>;

int main()
{
    std::cout << "Hello World!" << std::endl;

    ftree_type example_ftree;
    stxxl::fractal_tree::node<key_type, payload_type, comp_type, 4096> example_ftree_node;
    // mark variables unused, evades warnings
    tlx::unused(example_ftree);
    tlx::unused(example_ftree_node);

    return 0;
}