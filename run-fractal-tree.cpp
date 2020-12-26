/*
 * run-fractal-tree.cpp
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran     <hung@ae.cs.uni-frankfurt.de>
 */

#include <foxxll/mng/block_alloc_strategy.hpp>
#include "include/fractal_tree/fractal_tree.h"


using key_type = int;
using payload_type = int;
using ftree_type = stxxl::ftree<key_type, payload_type, 512, 2048>;

int main()
{

    ftree_type f_tree;

    return 0;
}