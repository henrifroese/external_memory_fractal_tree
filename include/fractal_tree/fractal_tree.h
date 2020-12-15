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
#include "fractal_tree_cache.h"
#include <unordered_map>
#include <unordered_set>

namespace stxxl {

namespace fractal_tree {

template <typename KeyType,
          typename DataType,
          size_t RawBlockSize,
          size_t RawMemoryPoolSize,
          typename AllocStr
         >
class fractal_tree {
    // Type declarations.
    using key_type = KeyType;
    using data_type = DataType;
    using value_type = std::pair<const key_type, data_type>;

    using self_type = fractal_tree<KeyType, DataType, RawBlockSize, RawMemoryPoolSize, AllocStr>;
    using bid_type = foxxll::BID<RawBlockSize>;

    // Nodes and Leaves declarations.
    using node_type = node<KeyType, DataType, RawBlockSize>;
    using leaf_type = leaf<KeyType, DataType, RawBlockSize>;

    using node_block_type = typename node_type::block_type;
    using leaf_block_type = typename leaf_type::block_type;

    // Caches for nodes and leaves.
    enum {
        num_blocks_in_leaf_cache = (RawMemoryPoolSize / 2) / RawBlockSize,
        num_blocks_in_node_cache = (RawMemoryPoolSize / 2) / RawBlockSize - 1 // -1 as root is always kept in cache
    };
    using node_cache_type = fractal_tree_cache<node_block_type, bid_type, num_blocks_in_node_cache>;
    using leaf_cache_type = fractal_tree_cache<leaf_block_type, bid_type, num_blocks_in_leaf_cache>;

private:
    std::unordered_map<int, node_type> m_node_id_to_node;
    std::unordered_map<int, leaf_type> m_leaf_id_to_leaf;

    int curr_node_id = 0;
    int curr_leaf_id = 0;

    std::unordered_set<bid_type> m_dirty_bids;

    node_cache_type m_node_cache = node_cache_type(m_dirty_bids);
    leaf_cache_type m_leaf_cache = leaf_cache_type(m_dirty_bids);

    node_type m_root;

public:
    fractal_tree() : m_root(curr_node_id++, bid_type()) {
        // Set up root.
        m_root.set_block(new node_block_type);
    };

    //! non-copyable: delete copy-constructor
    fractal_tree(const fractal_tree&) = delete;
    //! non-copyable: delete assignment operator
    fractal_tree& operator = (const fractal_tree&) = delete;

    ~fractal_tree() {
        delete m_node_cache;
        delete m_leaf_cache;
        delete m_root.get_block();
    }

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
        size_t RawBlockSize,
        size_t RawMemoryPoolSize,
        typename AllocStr
>
using ftree = fractal_tree::fractal_tree<KeyType, DataType, RawBlockSize, RawMemoryPoolSize, AllocStr>;

}

#endif //EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H