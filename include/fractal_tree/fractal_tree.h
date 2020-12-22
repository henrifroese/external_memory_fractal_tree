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

    static data_type dummy_datum = data_type();

private:
    std::unordered_map<int, node_type*> m_node_id_to_node;
    std::unordered_map<int, leaf_type*> m_leaf_id_to_leaf;

    int curr_node_id = 0;
    int curr_leaf_id = 0;

    std::unordered_set<bid_type> m_dirty_bids;

    node_cache_type m_node_cache = node_cache_type(m_dirty_bids);
    leaf_cache_type m_leaf_cache = leaf_cache_type(m_dirty_bids);

    node_type m_root;

    int m_depth = 1;

public:
    fractal_tree() : m_root(curr_node_id++, bid_type()) {
        // Set up root.
        m_root.set_block(new node_block_type);
        m_node_id_to_node.insert(std::pair<int, node_type>(m_root.get_id()), &m_root);
    };

    //! non-copyable: delete copy-constructor
    fractal_tree(const fractal_tree&) = delete;
    //! non-copyable: delete assignment operator
    fractal_tree& operator = (const fractal_tree&) = delete;

    ~fractal_tree() {
        delete m_node_cache;
        delete m_leaf_cache;
        delete m_root.get_block();
        for (auto it = m_leaf_id_to_leaf.begin(); it != m_leaf_id_to_leaf.end(); it++)
            delete (*it);
        for (auto it = m_node_id_to_node.begin(); it != m_node_id_to_node.end(); it++)
            delete (*it);
    }

    // Insert new key-datum pair into the tree
    void insert(const value_type& val) {
        if (m_root.buffer_full()) {
            // If we currently only have the root
            if (m_depth == 1)
                split_singular_root();
            else
                flush_buffer(m_root);
        }

        assert(!m_root.buffer_full());
        m_root.insert_into_buffer(val);
    }

    // TODO add const version (?)
    // First value of return is DummyData if key is not found.
    std::pair<data_type, bool> find(const key_type& key) {
        return recursive_find(m_root, key, 1);
    }

private:

    void load(node_type& node) {
        if (node != m_root) {
            bid_type node_bid = node.get_bid();
            node_block_type* cached_node_block = m_node_cache.load(node_bid);
            node.set_block(cached_node_block);
        }
    }

    void load(leaf_type& leaf) {
        bid_type leaf_bid = leaf.get_bid();
        leaf_block_type* cached_node_block = m_node_cache.load(leaf_bid);
        leaf.set_block(cached_node_block);
    }

    // Only have root and its buffer is full, so we split it up.
    void split_singular_root() {
        // Do what split_leaf will probably do similarly

        leaf_type& left_child = get_new_leaf();
        leaf_type& right_child = get_new_leaf();
        load(left_child);
        load(right_child);

        // Move left half of values in root buffer to left child buffer,
        // right halt to right child buffer.
        // Then promote mid item to key in root,
        // set child ids, and clear the root's buffer.

        // TODO Problem: how to move values to child buffers without copying? -> check how btree does it.
        std::vector<value_type> values_for_left_child = m_root.get_left_half_buffer_items();
        std::vector<value_type> values_for_right_child = m_root.get_right_half_buffer_items();

        // TODO implement those
        left_child.add_to_buffer(values_for_left_child);
        right_child.add_to_buffer(values_for_right_child);
    }

    node_type& get_new_node() {
        node_type* new_node = new node_type(curr_node_id++, bid_type());
        m_node_id_to_node.insert(std::pair<int, node_type>(new_node->get_id()), new_node);
        return *new_node;
    }

    leaf_type& get_new_leaf() {
        leaf_type* new_leaf = new leaf_type(curr_leaf_id++, bid_type());
        m_leaf_id_to_leaf.insert(std::pair<int, node_type>(new_leaf->get_id()), new_leaf);
        return *new_leaf;
    }

    std::pair<data_type, bool> recursive_find(node_type& curr_node, const key_type& key, int curr_depth) {
        /*
         * Pseudocode of function:
         *
         * if current node's buffer contains key:
         *      return (datum associated with the key, true)
         *
         * if current node is root node:
         *      return (dummy datum, false)
         *
         * if current node's values contain key:
         *      return (datum associated with the key, true)
         *
         * continue searching in correct child of current node
         */

        load(curr_node);

        // Search in buffer
        std::pair<data_type, bool> maybe_datum_and_found_in_buffer = curr_node.buffer_find(key);
        // If found
        if (maybe_datum_and_found_in_buffer.second)
            return maybe_datum_and_found_in_buffer;

        // Case: currently only have root
        if (m_depth == 1) {
            assert(curr_node == m_root);
            return std::pair<data_type, bool> (dummy_datum, false);
        }

        // Search in values
        // Return type <<datum of key if found else dummy_datum, id of child to go to>, bool whether key was found>
        std::pair<std::pair<data_type, int>, bool> maybe_datum_and_child_and_found_in_values = curr_node.values_find(key);
        // If found
        if (maybe_datum_and_child_and_found_in_values.second) {
            data_type datum = maybe_datum_and_child_and_found_in_values.first.first;
            return std::pair<data_type, bool> (datum, true);
        }

        // Continue in child
        int child_id = maybe_datum_and_child_and_found_in_values.first.second;

        // Child is leaf
        if (curr_depth == m_depth - 1)
            return leaf_find(*m_leaf_id_to_leaf.at(child_id), key);
        // Child is inner node
        else
            return recursive_find(*m_node_id_to_node.at(child_id), key, curr_depth+1);
    }

    std::pair<data_type, bool> leaf_find(leaf_type& curr_leaf, const key_type& key) {
        load(curr_leaf);

        // Search in buffer
        std::pair<data_type, bool> maybe_datum_and_found_in_buffer = curr_leaf.buffer_find(key);
        // If found
        if (maybe_datum_and_found_in_buffer.second)
            return maybe_datum_and_found_in_buffer;
        else
            return std::pair<data_type, bool> (dummy_datum, false);
    }

};

}

template <typename KeyType,
        typename DataType,
        size_t RawBlockSize,
        size_t RawMemoryPoolSize,
        DataType DummyDatum,
        typename AllocStr
>
using ftree = fractal_tree::fractal_tree<KeyType, DataType, RawBlockSize, RawMemoryPoolSize, DummyDatum, AllocStr>;

}

#endif //EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H