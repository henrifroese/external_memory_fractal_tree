/*
 * fractal_tree.h
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran <hung@ae.cs.uni-frankfurt.de>
 */

#ifndef EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H
#define EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H

#include <foxxll/mng/typed_block.hpp>
#include <foxxll/common/utils.hpp>
#include <foxxll/common/types.hpp>
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

    struct bid_hash {
        size_t operator () (const bid_type& bid) const {
            size_t result = foxxll::longhash1(bid.offset + reinterpret_cast<uint64_t>(bid.storage));
            return result;
        }
    };
    std::unordered_set<bid_type, bid_hash> m_dirty_bids;

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
        /*
         * We want to insert a new value into the tree.
         * Usually, this just means inserting it into
         * the root buffer.
         *
         * If the root buffer is full, we either
         * want to (a) split the root up (if the root
         * keys are at least half full) or (b) flush
         * the root buffer down to the root's children.
         *
         * Doing (a) keeps the "small-split invariant"
         * (see comments in flush_buffer for explanation)
         * intact, as we prevent the case that the root
         * overflows due to too many children splitting.
         */
        if (m_root.buffer_full()) {
            // If we currently only have the root
            if (m_depth == 1)
                split_singular_root();
            // Else: "normal" tree with not only root.
            // Proceed to flush the root's buffer.
            else {
                // Potentially split to keep "small-split invariant"
                if (m_root.values_at_least_half_full())
                    split_root();
                // Flush buffer
                else {
                    if (m_depth == 2)
                        flush_bottom_buffer(m_root);
                    else
                        flush_buffer(m_root, 1);
                }
            }
        }

        assert(!m_root.buffer_full());
        m_root.insert_into_buffer(val);
    }

    // First value of return is dummy if key is not found.
    std::pair<data_type, bool> find(const key_type& key) {
        return recursive_find(m_root, key, 1);
    }

private:

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

    // Split up the root in cases where we do not only
    // have the root (in that case: see split_singular_root).
    void split_root() {
        /*
         * Pseudocode:
         * 1. Create two new children nodes, left_child and right_child
         * 2. Move left half of root values to left child values,
         *    move right half of root values to right child values
         * 3. Move root buffer items to appropriate children
         * 4. Clear root buffer and values; promote mid item
         *    to values; set child ids
         *
         * 1+2+3 are done sequentially for each child to minimize
         * in-memory footprint.
         */
        // Gather buffer items / values / nodeIDs to distribute to the children
        std::vector<value_type> values_for_left_child = m_root.get_left_half_values();
        std::vector<value_type> values_for_right_child = m_root.get_right_half_values();
        std::vector<int> nodeIDs_for_left_child = m_root.get_left_half_nodeIDs();
        std::vector<int> nodeIDs_for_right_child = m_root.get_right_half_nodeIDs();

        value_type mid_value = m_root.get_mid_value();
        std::vector<value_type> buffer_items_for_left_child = m_root.get_left_buffer_items_for_split(mid_value);
        std::vector<value_type> buffer_items_for_right_child = m_root.get_right_buffer_items_for_split(mid_value);

        // Create new left child and populate it
        node_type& left_child = get_new_node();
        load(left_child);
        m_dirty_bids.insert(left_child.get_bid());

        left_child.set_buffer(buffer_items_for_left_child);
        left_child.set_values(values_for_left_child, nodeIDs_for_left_child);

        // Create new right child and populate it
        node_type& right_child = get_new_node();
        load(right_child);
        m_dirty_bids.insert(right_child.get_bid());

        right_child.set_buffer(buffer_items_for_right_child);
        right_child.set_values(values_for_right_child, nodeIDs_for_right_child);

        // Update root
        m_root.clear_buffer();
        m_root.clear_values();
        m_root.add_to_values(mid_value, left_child.get_id(), right_child.get_id());
        m_depth++;
    }

    // Only have root and its buffer is full, so we split it up.
    void split_singular_root() {
        /* Move left half of values in root buffer to left child buffer,
         * right half to right child buffer.
         * Then promote mid item to key in root,
         * set child ids, and clear the root's buffer.
        */
        // Left child
        leaf_type& left_child = get_new_leaf();
        load(left_child);
        m_dirty_bids.insert(left_child.get_bid());

        std::vector<value_type> values_for_left_child = m_root.get_left_half_buffer_items();
        left_child.add_to_buffer(values_for_left_child);

        // Right child
        leaf_type& right_child = get_new_leaf();
        load(right_child);
        m_dirty_bids.insert(right_child.get_bid());

        std::vector<value_type> values_for_right_child = m_root.get_right_half_buffer_items();
        right_child.add_to_buffer(values_for_right_child);

        // Update root
        value_type mid_value = m_root.get_mid_buffer_item();
        m_root.add_to_values(mid_value, left_child.get_id(), right_child.get_id());
        m_root.clear_buffer();
        m_depth++;
    }

    void split(node_type& parent_node, leaf_type& left_child, int low, int high) {
        std::vector<value_type> combined_values = merge_into<value_type>(
                parent_node.get_buffer_items(low, high), left_child.get_buffer_items());

        int mid = (combined_values.size() - 1) / 2;
        value_type mid_value = combined_values.at(mid);

        // Add to left child buffer
        left_child.clear_buffer();
        left_child.add_to_buffer(
                std::vector<value_type>(
                        combined_values.begin(), combined_values.begin() + 2
                ));

        // Create new right child, add to buffer,
        leaf_type& right_child = get_new_leaf();
        load(right_child);
        right_child.add_to_buffer(
                std::vector<value_type>(
                        combined_values.begin() + mid + 1, combined_values.end()
                ));

        // Register children with parent
        parent_node.add_to_values(mid_value, left_child.get_id(), right_child.get_id());
    }

    // Split left_child into two nodes
    void split(node_type& parent_node, node_type& left_child) {
        /*
         * Pseudocode:
         * 1. Create new right child (the left child that should be split
         *    will be reused)
         * 2. Move right half of root values to right child values
         * 3. Move appropriate buffer items to right child
         * 4. Promote mid item to value of parent nodes;
         *    set child ids
        */
        // Gather buffer items / values / nodeIDs to distribute to the children

        std::vector<value_type> values_for_left_child = left_child.get_left_half_values();
        std::vector<value_type> values_for_right_child = left_child.get_right_half_values();
        std::vector<int> nodeIDs_for_left_child = left_child.get_left_half_nodeIDs();
        std::vector<int> nodeIDs_for_right_child = left_child.get_right_half_nodeIDs();

        value_type mid_value = left_child.get_mid_value();
        std::vector<value_type> buffer_items_for_left_child = left_child.get_left_buffer_items_for_split(mid_value);
        std::vector<value_type> buffer_items_for_right_child = left_child.get_right_buffer_items_for_split(mid_value);

        // Create new right child and populate it
        node_type& right_child = get_new_node();
        load(right_child);
        m_dirty_bids.insert(right_child.get_bid());

        right_child.set_buffer(buffer_items_for_right_child);
        right_child.set_values(values_for_right_child, nodeIDs_for_right_child);

        // Set values for left child
        m_dirty_bids.insert(left_child.get_bid());
        left_child.set_buffer(buffer_items_for_left_child);
        left_child.set_values(values_for_left_child, nodeIDs_for_left_child);

        // Update parent
        parent_node.add_to_values(mid_value, left_child.get_id(), right_child.get_id());
        m_dirty_bids.insert(parent_node);
    }

    // Flush the items in a node's buffer to the node's children
    void flush_buffer(node_type& curr_node, int curr_depth) {
        /*
         * flush_buffer is only called on nodes with a full buffer.
         * We want to flush curr_node's full buffer down to its children.
         * If this leads to a full buffer in a child, flush_buffer
         * is called recursively on the child.
         *
         * This procedure could lead to leaves at the bottom overflowing
         * and thus splitting, which could in turn lead to their parent
         * nodes splitting, etc. To prevent the situation that in the
         * middle of flushing curr_node's buffer, so many of curr_node's
         * children need to split that curr_node itself needs to split,
         * we maintain the small-split invariant as described in
         * https://dspace.mit.edu/handle/1721.1/37084 .
         *
         * To maintain the invariant, all children with >= floor(b/2)+1
         * children are first split before flush_buffer is called on them
         * (for the root, this is done in the insert function that
         * first calls flush_buffer).
         * Thus, curr_node has at most floor(b/2) children and
         * gets at most 1 key from each child, so curr_node gets
         * at most floor(b/2) + floor(b/2) <= b children, and
         * does not need to split again.
         *
         * Pseudocode for flush_buffer (here, flush_buffer
         * is split into a function for the case that
         * children are nodes (here) and children are
         * leaves (flush_bottom_buffer)):
         *
         * num_children = curr_node.num_children(); high=0; child_index=0;
         * // distribute buffer items to children
         * while child_index < num_children:
         *      child = curr_node.child(child_index);
         *
         *      if !child.is_leaf() && child.num_children() >= floor(b/2)+1:
         *          split child; // maintain small-split invariant
         *
         *      low = high;
         *      high = index s.t. [buffer[low], ..., buffer[high]) should go to current child;
         *
         *      if child is a leaf:
         *          if pushing the items to it will lead to an overflow:
         *              combine leaf items and items to push down;
         *              split combined items into two leaves;
         *              promote middle item to key of curr_node;
         *          else:
         *              push items down
         *      else:
         *          // Note that the child cannot split as
         *          // we maintained the small-split invariant above
         *          push items until child buffer is full;
         *          flush_buffer(child);
         *          push remaining items, if there are any;
         *
         *      child_index++;
         *      // num children can change due to splitting
         *      num_children = curr_node.num_children();
         */
        load(curr_node);
        int num_children = curr_node.num_children();
        int low, high = 0;
        // Note that we cannot iterate through the children
        // as the curr_node might be kicked to external memory
        // during a recursive call to a child's flush_buffer,
        // so we have to work with indexes.
        int child_index = 0;
        node_type child;

        while (child_index < num_children) {
            child = m_node_id_to_node.find(curr_node.get_child_id(child_index));
            load(child);

            if (child.values_at_least_half_full()) {
                split(curr_node, child);
                m_dirty_bids.add(child.get_bid());
                m_dirty_bids.add(curr_node.get_bid());
            }

            low = high;
            high = curr_node.index_of_upper_bound_of_buffer(child_index);

            int num_items_to_push = high - low;
            int space_in_child_buffer = child.max_buffer_size() - child.num_items_in_buffer();

            if (num_items_to_push > space_in_child_buffer) {
                // Again we cannot keep the items to push down in-memory as
                // that could lead to stack overflows in recursive calls to
                // flush_buffer -> use appropriate scoping.

                // Push first part.
                {
                    std::vector<value_type> items_to_push = curr_node.get_buffer_items(
                            low, low + space_in_child_buffer
                            );
                    child.add_to_buffer(items_to_push);
                }
                m_dirty_bids.add(child.get_bid());
                // Flush child buffer.
                if (curr_depth == m_depth - 1)
                    flush_bottom_buffer(child);
                else
                    flush_buffer(child, curr_depth+1);

                // Reload (nodes might have been kicked out of memory
                // in the recursive call to flush_buffer)
                load(child);
                load(curr_node);
                // Push second part.
                {
                    std::vector<value_type> items_to_push = curr_node.get_buffer_items(
                            low + space_in_child_buffer, high
                    );
                    child.add_to_buffer(items_to_push);
                }
                m_dirty_bids.add(child.get_bid());

            } else {
                std::vector<value_type> items_to_push = curr_node.get_buffer_items(low, high);
                child.add_to_buffer(items_to_push);
                m_dirty_bids.add(child.get_bid());
            }

            child_index++;
            // num_children can change due to splitting
            num_children = curr_node.num_children();
        }
        curr_node.clear_buffer();
        m_dirty_bids.add(curr_node.get_bid());
    }

    // See flush_buffer
    void flush_bottom_buffer(node_type& curr_node) {
        load(curr_node);
        int num_children = curr_node.num_children();
        int low, high = 0;
        int child_index = 0;
        leaf_type child;

        while (child_index < num_children) {
            child = m_leaf_id_to_leaf.find(curr_node.get_child_id(child_index));
            load(child);

            low = high;
            high = curr_node.index_of_upper_bound_of_buffer(child_index);

            int num_items_to_push = high - low;
            // If pushing the items to the child would lead to an overflow ...
            if (child.num_buffer_items() + num_items_to_push > child.max_buffer_size()) {
                split(curr_node, child, low, high);
                m_dirty_bids.add(curr_node.get_bid());
                m_dirty_bids.add(child.get_bid());
            // Else: just push down
            } else {
                std::vector<value_type> buffer_items_to_push_down = curr_node.get_buffer_items(low, high);
                child.add_to_buffer(buffer_items_to_push_down);
                m_dirty_bids.add(child.get_bid());
            }

            child_index++;
            // num_children can change due to splitting
            num_children = curr_node.num_children();
        }
        curr_node.clear_buffer();
        m_dirty_bids.add(curr_node.get_bid());
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
using ftree = fractal_tree::fractal_tree<KeyType, DataType, RawBlockSize, RawMemoryPoolSize, AllocStr>;

}

#endif //EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H