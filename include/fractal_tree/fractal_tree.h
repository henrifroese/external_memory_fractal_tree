/*
 * fractal_tree.h
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran <hung@ae.cs.uni-frankfurt.de>
 */

#ifndef EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H
#define EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H

#include <foxxll/mng/typed_block.hpp>
#include "node.h"
#include <unordered_map>

namespace stxxl {

namespace fractal_tree {

template <typename KeyType,
          typename DataType,
          unsigned RawBlockSize,
          typename AllocStrategy>
class fractal_tree {
    // Type declarations.
    using key_type = KeyType;
    using data_type = DataType;
    using value_type = std::pair<const key_type, data_type>;

    using self_type = fractal_tree<KeyType, DataType, RawBlockSize, AllocStrategy>;
    using bid_type = foxxll::BID<RawBlockSize>;

    using node_type = node<KeyType, DataType, RawBlockSize>;
    using leaf_type = leaf<KeyType, DataType, RawBlockSize>;

    using node_block_type = typename node_type::block_type;
    using leaf_block_type = typename leaf_type::block_type;

private:
    std::unordered_map<int, node_type> node_id_to_node;
    std::unordered_map<int, leaf_type> node_id_to_leaf;

    /*
     * Next steps:
     *  - cache like in pq implementation
     *  - id counters in fractal tree for nodes and leafs
     *  - idea: tree wants new node -> creates BID and ID and constructs node with those, registers node in hashmap -> writes to node -> writes to cache
     *          (problem: need to read into a typed_block object, but only want to use element at 0. Should change pointer in node and leaf classes to
     *                    correct typed_block and then in all functions only access the 0th element.)
     */
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
          unsigned RawBlockSize,
          typename AllocStrategy
          >
using ftree = fractal_tree::fractal_tree<KeyType, DataType, KeyCompareWithMaxType, RawBlockSize, AllocStrategy>;

}

#endif //EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H