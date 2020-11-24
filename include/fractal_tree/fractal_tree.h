/*
 * fractal_tree.h
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran <hung@ae.cs.uni-frankfurt.de>
 */

#ifndef EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H
#define EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H

#include "node.h"

namespace stxxl {

namespace fractal_tree {

template <typename KeyType,
          typename DataType,
          typename KeyCompareWithMaxType,
          unsigned RawNodeSize,
          unsigned RawLeafSize,
          typename AllocStrategy>
class fractal_tree {
    using key_type = KeyType;
    using data_type = DataType;
    using key_compare = KeyCompareWithMaxType;
    using value_type = std::pair<const key_type, data_type>;
    using node_type = node<key_type, data_type, key_compare, RawNodeSize>;

public:
    // TODO remove
    fractal_tree() = default;

    //! non-copyable: delete copy-constructor
    fractal_tree(const fractal_tree&) = delete;
    //! non-copyable: delete assignment operator
    fractal_tree& operator = (const fractal_tree&) = delete;

    // TODO should return std::pair<iterator, bool>, iterator and whether key was found while inserting
    void insert(const value_type& x)
    {

    }

    // TODO should return iterator
    bool find(const key_type& k)
    {
        return true;
    }
};

}

template <typename KeyType,
          typename DataType,
          typename KeyCompareWithMaxType,
          unsigned RawNodeSize,
          unsigned RawLeafSize,
          typename AllocStrategy
          >
using ftree = fractal_tree::fractal_tree<KeyType, DataType, KeyCompareWithMaxType, RawNodeSize, RawLeafSize, AllocStrategy>;

}

#endif //EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H