/*
 * fractal_tree.h
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran <hung@ae.cs.uni-frankfurt.de>
 */

#ifndef EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H
#define EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H

#include <tlx/logger.hpp>
#include <foxxll/mng/typed_block.hpp>
#include <foxxll/common/utils.hpp>
#include <foxxll/common/types.hpp>
#include "node.h"
#include "fractal_tree_cache.h"
#include <unordered_map>
#include <unordered_set>
#include <foxxll/mng/block_manager.hpp>

namespace stxxl {

namespace fractal_tree {

template <typename KeyType,
          typename DataType,
          size_t RawBlockSize,
          size_t RawMemoryPoolSize,
          typename AllocStr
         >
class fractal_tree {

    static constexpr bool debug = true;

    // Type declarations.
    using key_type = KeyType;
    using data_type = DataType;
    using value_type = std::pair<key_type, data_type>;

    using self_type = fractal_tree<KeyType, DataType, RawBlockSize, RawMemoryPoolSize, AllocStr>;
    using bid_type = foxxll::BID<RawBlockSize>;
    using alloc_strategy_type = AllocStr;

    // Nodes and Leaves declarations.
    using node_type = node<KeyType, DataType, RawBlockSize>;
    using leaf_type = leaf<KeyType, DataType, RawBlockSize>;

    using node_block_type = typename node_type::block_type;
    using leaf_block_type = typename leaf_type::block_type;

public:
    enum {
        max_num_buffer_items_in_node = node_type::max_num_buffer_items_in_node,
        max_num_values_in_node = node_type::max_num_values_in_node,
        max_num_buffer_items_in_leaf = leaf_type::max_num_buffer_items_in_leaf,
        node_buffer_mid = (max_num_buffer_items_in_node - 1) / 2,
        node_values_mid = (max_num_values_in_node - 1) / 2,
        leaf_buffer_mid = (max_num_buffer_items_in_leaf - 1) / 2,
    };
    // We want to be able to split
    // a node with floor((max_num_values_in_node-1)/2) values. Thus,
    // as at least 3 values are needed to be able to split,
    // we require max_num_values_in_nodes >= 7
    static_assert(max_num_values_in_node >= 7, "RawBlockSize too small -> too few values per node!");

private:
    // Caches for nodes and leaves.
    static_assert(RawMemoryPoolSize / 2 >= RawBlockSize, "RawMemoryPoolSize too small -> too few nodes fit in cache!");
    enum {
        num_blocks_in_leaf_cache = (RawMemoryPoolSize / 2) / RawBlockSize,
        // -1 as root is always kept in cache
        num_blocks_in_node_cache = (RawMemoryPoolSize / 2) / RawBlockSize - 1
    };
    static_assert(num_blocks_in_leaf_cache >= 2, "RawMemoryPoolSize too small -> less than 2 leaves fit in leaf cache!");
    static_assert(num_blocks_in_node_cache >= 2, "RawMemoryPoolSize too small -> less than 2 nodes fit in node cache!");

    struct bid_hash {
        size_t operator () (const bid_type& bid) const {
            size_t result = foxxll::longhash1(bid.offset + reinterpret_cast<uint64_t>(bid.storage));
            return result;
        }
    };

    using node_cache_type = fractal_tree_cache<node_block_type, bid_type, bid_hash, num_blocks_in_node_cache>;
    using leaf_cache_type = fractal_tree_cache<leaf_block_type, bid_type, bid_hash, num_blocks_in_leaf_cache>;

    static constexpr data_type dummy_datum() { return data_type(); };

private:
    std::unordered_map<int, node_type*> m_node_id_to_node;
    std::unordered_map<int, leaf_type*> m_leaf_id_to_leaf;

    std::unordered_set<bid_type, bid_hash> m_dirty_bids;

    node_cache_type m_node_cache = node_cache_type(m_dirty_bids);
    leaf_cache_type m_leaf_cache = leaf_cache_type(m_dirty_bids);

    int m_curr_node_id = 0;
    int m_curr_leaf_id = 0;
    int m_depth = 1;

    node_type m_root;
    foxxll::block_manager* bm = foxxll::block_manager::get_instance();
    alloc_strategy_type m_alloc_strategy;


public:
    fractal_tree() :
        m_root(m_curr_node_id++, bid_type()) {
        // Set up root.
        m_root.set_block(new node_block_type);
        m_node_id_to_node.insert(std::pair<int, node_type*>(m_root.get_id(), &m_root));

        TLX_LOG << "sizeof(KeyType):\t" << sizeof(KeyType) << "\tBytes";
        TLX_LOG << "sizeof(DataType):\t" << sizeof(DataType) << "\tBytes";
        TLX_LOG << "sizeof(node_block_type):\t" << sizeof(node_block_type) << "\tBytes";
        TLX_LOG << "sizeof(leaf_block_type):\t" << sizeof(leaf_block_type) << "\tBytes";
        TLX_LOG << "sizeof(actual node block):\t" << sizeof(typename node_type::node_block) << "\tBytes";
        TLX_LOG << "sizeof(actual leaf block):\t" << sizeof(typename leaf_type::leaf_block) << "\tBytes";
        TLX_LOG << "Wasted bytes:\t" <<
                    sizeof(node_block_type) + sizeof(leaf_block_type)
                    - sizeof(typename node_type::node_block) - sizeof(typename leaf_type::leaf_block)
                    << "\tBytes";
        TLX_LOG << "RawBlockSize:\t" << RawBlockSize << "\tBytes";
        TLX_LOG << "RawMemoryPoolSize:\t" << RawMemoryPoolSize << "\tBytes";
        TLX_LOG << "Max number of buffer items per node:\t" << max_num_buffer_items_in_node;
        TLX_LOG << "Max number of values per node:\t" << max_num_values_in_node;
        TLX_LOG << "Max number of children per node:\t" << max_num_values_in_node+1;
        TLX_LOG << "Max number of items per leaf:\t" << max_num_buffer_items_in_leaf;

        TLX_LOG1 << "Number of leaves that fit in leaf cache:\t" << num_blocks_in_leaf_cache;
        TLX_LOG1 << "Number of nodes that fit in node cache:\t" << num_blocks_in_node_cache;
    }

    //! non-copyable: delete copy-constructor
    fractal_tree(const fractal_tree&) = delete;
    //! non-copyable: delete assignment operator
    fractal_tree& operator = (const fractal_tree&) = delete;

    ~fractal_tree() {
        // Delete the root node's block
        // (not in cache).
        delete m_root.get_block();

        // Delete the node objects (not the
        // actual data blocks, those are
        // managed by the cache).
        for (auto it = m_leaf_id_to_leaf.begin(); it != m_leaf_id_to_leaf.end(); it++)
            delete it->second;
        for (auto it = m_node_id_to_node.begin(); it != m_node_id_to_node.end(); it++) {
            if (*(it->second) != m_root)
                delete it->second;
        }
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
         * See flush_buffer for more explanations.
         */
        if (m_root.buffer_full()) {
            // If we currently only have the root ...
            if (m_depth == 1)
                split_singular_root();
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
                    assert(m_root.buffer_empty());
                }
            }
        }
        assert(!m_root.buffer_full());
        m_root.add_to_buffer(val);
    }

    // First value of return is dummy if key is not found.
    std::pair<data_type, bool> find(key_type key) {
        return recursive_find(m_root, key, 1);
    }

    int depth() const {
        return m_depth;
    }
    
    int num_nodes() const {
        return m_curr_node_id;
    }

    int num_leaves() const {
        return m_curr_leaf_id;
    }

    std::vector<value_type> range_find(key_type lower, key_type upper) {
        std::vector<value_type> result {};
        // Guess
        result.reserve(max_num_buffer_items_in_leaf * 10);

        // Flush root buffer first
        if (m_depth > 1) {
            // Potentially split to keep "small-split invariant"
            if (m_root.values_at_least_half_full())
                split_root();
            // Flush buffer
            else {
                if (m_depth == 2)
                    flush_bottom_buffer(m_root);
                else
                    flush_buffer(m_root, 1);
                assert(m_root.buffer_empty());
            }
        }

        recursive_range_find(m_root, lower, upper, 1, result);

        return result;
    }

    void visualize() {
        if (num_nodes() > 30) {
            std::cout << "Tree is too large to visualize" << std::endl;
            return;
        }
        std::cout << "VISUALIZING TREE...\n" << std::endl;
        std::cout << "Depth: " << m_depth << std::endl;
        std::cout << "Number of nodes: " << m_curr_node_id << std::endl;
        std::cout << "Number of leaves: " << m_curr_leaf_id << std::endl;

        std::cout << "Level-order traversal of tree:\n" << std::endl;

        std::vector<int> level_ids { 0 };
        int curr_depth = 1;

        while (curr_depth <= m_depth) {
            // Case: currently looking at inner nodes
            if ((curr_depth < m_depth) || (m_depth == 1)) {
                std::vector<int> next_level_ids {};

                for (auto id : level_ids) {
                    node_type n = *m_node_id_to_node.at(id);
                    load(n);

                    // Pretty-print keys, and first and last buffer item
                    std::cout << "[ ";
                    for (value_type val : n.get_values())
                        std::cout << val.first << ", ";
                    std::cout << " | ";
                    if (!n.buffer_empty()) {
                        std::vector<value_type> buffer_items = n.get_buffer_items();
                        std::cout << std::min_element(buffer_items.begin(), buffer_items.end())->first << " ... ";
                        std::cout << std::max_element(buffer_items.begin(), buffer_items.end())->first;
                    }
                    std::cout << " ]    ";

                    // Add children to next level
                    for (int i=0; i<n.num_children(); i++)
                        next_level_ids.push_back(n.get_child_id(i));
                }
                level_ids = next_level_ids;
                std::cout << std::endl;
            }
            // Case: currently looking at leaves
            else {
                for (auto id : level_ids) {
                    leaf_type n = *m_leaf_id_to_leaf.at(id);
                    load(n);

                    // Pretty-print first and last buffer item
                    std::cout << "[ ";
                    std::vector<value_type> buffer_items = n.get_buffer_items();
                    std::cout << std::min_element(buffer_items.begin(), buffer_items.end())->first << " ... ";
                    std::cout << std::max_element(buffer_items.begin(), buffer_items.end())->first;
                    std::cout << " ]    ";
                }
                std::cout << std::endl;
            }

            curr_depth++;
        }

    }

private:

    node_type& get_new_node() {
        auto* new_node = new node_type(m_curr_node_id++, bid_type());
        m_node_id_to_node.insert(std::pair<int, node_type*>(new_node->get_id(), new_node));
        bm->new_block(m_alloc_strategy, new_node->get_bid());
        return *new_node;
    }

    leaf_type& get_new_leaf() {
        auto* new_leaf = new leaf_type(m_curr_leaf_id++, bid_type());
        m_leaf_id_to_leaf.insert(std::pair<int, leaf_type*>(new_leaf->get_id(), new_leaf));
        bm->new_block(m_alloc_strategy, new_leaf->get_bid());
        return *new_leaf;
    }

    void load(node_type& node) {
        if (node != m_root) {
            bid_type& node_bid = node.get_bid();
            node_block_type* cached_node_block = m_node_cache.load(node_bid);
            node.set_block(cached_node_block);
        }
    }

    void load(leaf_type& leaf) {
        bid_type& leaf_bid = leaf.get_bid();
        leaf_block_type* cached_node_block = m_leaf_cache.load(leaf_bid);
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
        int values_mid = (m_root.num_values() - 1) / 2;

        std::vector<value_type> values_for_left_child = m_root.get_values(
                0, values_mid
        );
        std::vector<value_type> values_for_right_child = m_root.get_values(
                values_mid + 1, m_root.num_values()
        );
        std::vector<int> nodeIDs_for_left_child = m_root.get_nodeIDs(
                0, values_mid + 1
        );
        std::vector<int> nodeIDs_for_right_child = m_root.get_nodeIDs(
                values_mid + 1, m_root.num_children()
        );

        value_type mid_value = m_root.get_value(values_mid);
        std::vector<value_type> buffer_items_for_left_child = m_root.get_buffer_items_less_than(mid_value);
        std::vector<value_type> buffer_items_for_right_child = m_root.get_buffer_items_greater_equal_than(mid_value);

        // Create new left child and populate it
        node_type& left_child = get_new_node();
        load(left_child);
        m_dirty_bids.insert(left_child.get_bid());

        left_child.set_values_and_nodeIDs(values_for_left_child, nodeIDs_for_left_child);
        left_child.set_buffer(buffer_items_for_left_child);

        // Create new right child and populate it
        node_type& right_child = get_new_node();
        load(right_child);
        m_dirty_bids.insert(right_child.get_bid());

        right_child.set_values_and_nodeIDs(values_for_right_child, nodeIDs_for_right_child);
        right_child.set_buffer(buffer_items_for_right_child);

        // Update root
        m_root.clear_buffer();
        m_root.clear_values();
        m_root.add_to_values(mid_value, left_child.get_id(), right_child.get_id());
        m_depth++;
    }

    // Only have root and its buffer is full, so we split it up.
    void split_singular_root() {
        /* Pseudocode:
         * 1. Create two new children leaves
         * 2. Move left half of values in root buffer to left child buffer,
         *    right half to right child buffer
         * 3. Promote mid item to key in root,
         *    set child ids, and clear the root's buffer.
        */
        // Left child
        leaf_type& left_child = get_new_leaf();
        load(left_child);
        m_dirty_bids.insert(left_child.get_bid());

        std::vector<value_type> values_for_left_child = m_root.get_buffer_items(
                0, node_buffer_mid
                );
        left_child.set_buffer(values_for_left_child);

        // Right child
        leaf_type& right_child = get_new_leaf();
        load(right_child);
        m_dirty_bids.insert(right_child.get_bid());

        std::vector<value_type> values_for_right_child = m_root.get_buffer_items(
                node_buffer_mid+1, max_num_buffer_items_in_node
        );
        right_child.set_buffer(values_for_right_child);

        // Update root
        value_type mid_value = m_root.get_buffer_item(node_buffer_mid);
        m_root.add_to_values(mid_value, left_child.get_id(), right_child.get_id());
        m_root.clear_buffer();
        m_depth++;
    }

    // Combine the buffer items in left_child and from
    // parent_node.buffer[low], ..., parent_node.buffer[high-1],
    // and distribute them to left_child and a new right_child.
    // left_child must be the child of parent_node.
    // low, high are indexes s.t. parent_node.buffer[low], ..., parent_node.buffer[high-1]
    // belong to the left_child.
    void split_and_flush(node_type& parent_node, leaf_type& left_child, int low, int high) {
        /*
         * During flushing the buffer of parent_node, we want to flush
         * parent_node.buffer[low], ..., parent_node.buffer[high-1]
         * to left_child. However, left_child does not have enough
         * enough space in its buffer for all the items. In that
         * situation, this function is called. We now have
         * the buffer items in left_child's buffer, and
         * the ones that the parent_node wants to flush
         * down. Those are combined, and then the left
         * half is given to the left_child, the right
         * half is given to a new right_child, and the
         * mid item is promoted to
         *
         */
        load(parent_node);
        load(left_child);
        // Combine the sorted buffer items, and take from the parent
        // in case of duplicates.
        std::vector<value_type> combined_values = merge_into<value_type>(
                parent_node.get_buffer_items(low, high), left_child.get_buffer_items());

        if (combined_values.empty())
            return;

        int mid = (combined_values.size() - 1) / 2;
        value_type mid_value = combined_values.at(mid);

        // Add to left child buffer
        left_child.clear_buffer();
        std::vector<value_type> buffer_items_for_left_child =
                std::vector<value_type>(
                        combined_values.begin(), combined_values.begin() + mid
                );
        left_child.set_buffer(buffer_items_for_left_child);
        m_dirty_bids.insert(left_child.get_bid());

        // Create new right child, add to buffer,
        leaf_type& right_child = get_new_leaf();
        load(right_child);
        m_dirty_bids.insert(right_child.get_bid());

        std::vector<value_type> buffer_items_for_right_child =
                std::vector<value_type>(
                        combined_values.begin() + mid + 1, combined_values.end()
                );
        right_child.set_buffer(buffer_items_for_right_child);

        // Register children with parent
        parent_node.add_to_values(mid_value, left_child.get_id(), right_child.get_id());
        m_dirty_bids.insert(parent_node.get_bid());
    }

    // Split left_child of parent_node into two nodes
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
        load(parent_node);
        load(left_child);
        // Gather buffer items / values / nodeIDs to distribute to the children
        int values_mid = (left_child.num_values() - 1) / 2;

        std::vector<value_type> values_for_left_child = left_child.get_values(
                0, values_mid
                );
        std::vector<value_type> values_for_right_child = left_child.get_values(
                values_mid + 1, left_child.num_values()
                );
        std::vector<int> nodeIDs_for_left_child = left_child.get_nodeIDs(
                0, values_mid + 1
                );
        std::vector<int> nodeIDs_for_right_child = left_child.get_nodeIDs(
                values_mid + 1, left_child.num_values() + 1
        );

        value_type mid_value = left_child.get_value(values_mid);
        std::vector<value_type> buffer_items_for_left_child = left_child.get_buffer_items_less_than(mid_value);
        std::vector<value_type> buffer_items_for_right_child = left_child.get_buffer_items_greater_equal_than(mid_value);

        // Create new right child and populate it
        node_type& right_child = get_new_node();
        load(right_child);
        m_dirty_bids.insert(right_child.get_bid());

        right_child.set_values_and_nodeIDs(values_for_right_child, nodeIDs_for_right_child);
        right_child.set_buffer(buffer_items_for_right_child);

        // Set values for left child
        m_dirty_bids.insert(left_child.get_bid());
        left_child.set_values_and_nodeIDs(values_for_left_child, nodeIDs_for_left_child);
        left_child.set_buffer(buffer_items_for_left_child);

        // Update parent
        parent_node.add_to_values(mid_value, left_child.get_id(), right_child.get_id());
        m_dirty_bids.insert(parent_node.get_bid());
    }

    // Flush the items in a node's full buffer to the node's children
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
         * num_children = curr_node.num_children();
         * high=0, child_index=0;
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

        while (child_index < num_children) {
            low = high;
            high = curr_node.index_of_upper_bound_of_buffer(child_index);

            int num_items_to_push = high - low;

            if (num_items_to_push == 0) {
                child_index++;
                continue;
            }

            auto it = m_node_id_to_node.find(curr_node.get_child_id(child_index));
            assert(it != m_node_id_to_node.end());
            node_type& child = *(it->second);
            load(child);
            load(curr_node);

            if (child.values_at_least_half_full()) {
                split(curr_node, child);
                // After splitting, the child is now responsible
                // for a different range of values, so we have to
                // re-calculate what should be pushed down.
                load(curr_node);
                load(child);
                high = curr_node.index_of_upper_bound_of_buffer(child_index);
                num_items_to_push = high - low;
            }

            int space_in_child_buffer = child.max_buffer_size() - child.num_items_in_buffer();

            if (num_items_to_push > space_in_child_buffer) {
                // Here we cannot keep the items to push down in-memory as
                // that could lead to stack overflows in recursive calls to
                // flush_buffer -> use appropriate scoping.

                // Push first part.
                {
                    std::vector<value_type> items_to_push = curr_node.get_buffer_items(
                            low, low + space_in_child_buffer
                            );
                    child.add_to_buffer(items_to_push);
                }
                m_dirty_bids.insert(child.get_bid());
                // Flush child buffer.
                if (curr_depth == m_depth - 2)
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
                m_dirty_bids.insert(child.get_bid());

            } else {
                std::vector<value_type> items_to_push = curr_node.get_buffer_items(low, high);
                child.add_to_buffer(items_to_push);
                m_dirty_bids.insert(child.get_bid());
            }

            child_index++;
            // num_children can change due to splitting
            num_children = curr_node.num_children();
        }
        curr_node.clear_buffer();
        m_dirty_bids.insert(curr_node.get_bid());
    }

    // See flush_buffer
    void flush_bottom_buffer(node_type& curr_node) {
        load(curr_node);
        int num_children = curr_node.num_children();
        int low, high = 0;
        int child_index = 0;

        while (child_index < num_children) {
            low = high;
            high = curr_node.index_of_upper_bound_of_buffer(child_index);

            int num_items_to_push = high - low;

            if (num_items_to_push == 0) {
                child_index++;
                continue;
            }

            auto it = m_leaf_id_to_leaf.find(curr_node.get_child_id(child_index));
            assert(it != m_leaf_id_to_leaf.end());
            leaf_type& child = *(it->second);
            load(child);
            load(curr_node);

            // If pushing the items to the child would lead to an overflow ...
            if (child.num_items_in_buffer() + num_items_to_push > child.max_buffer_size())
                split_and_flush(curr_node, child, low, high);
            // Else: just push down
            else {
                std::vector<value_type> buffer_items_to_push_down = curr_node.get_buffer_items(low, high);
                child.add_to_buffer(buffer_items_to_push_down);
                m_dirty_bids.insert(child.get_bid());
            }
            load(curr_node);

            child_index++;
            // num_children can change due to splitting
            num_children = curr_node.num_children();
        }
        curr_node.clear_buffer();
        m_dirty_bids.insert(curr_node.get_bid());
    }

    void recursive_range_find(node_type& curr_node, key_type& lower, key_type& upper, int curr_depth, std::vector<value_type>& result) {
        // Flush buffer
        if (curr_depth == m_depth - 1) {
            flush_bottom_buffer(curr_node);
        } else {
            flush_buffer(curr_node, curr_depth);
        }
        load(curr_node);

        std::vector<value_type> values = curr_node.get_values();
        std::vector<int> nodeIDs = curr_node.get_nodeIDs(0, curr_node.num_children());

        bool next_level_is_leaf = curr_depth == m_depth - 1;

        for (int i=0; i < values.size(); i++) {
            // Look at i-th value and descendants
            if (values[i].first == lower) {
                result.push_back(values[i]);
            }

            if ((values[i].first > lower) && (values[i].first <= upper)) {
                if (next_level_is_leaf)
                    recursive_range_find_leaf(*m_leaf_id_to_leaf.at(nodeIDs[i]), lower, upper, result);
                else
                    recursive_range_find(*m_node_id_to_node.at(nodeIDs[i]), lower, upper, curr_depth+1, result);

                result.push_back(values[i]);
            }

            if (values[i].first > upper) {
                if (next_level_is_leaf)
                    recursive_range_find_leaf(*m_leaf_id_to_leaf.at(nodeIDs[i]), lower, upper, result);
                else
                    recursive_range_find(*m_node_id_to_node.at(nodeIDs[i]), lower, upper, curr_depth+1, result);

                break;
            }

        }
        // Potentially look at last child
        if ((!values.empty()) && (values[values.size()-1].first < upper)) {
            if (next_level_is_leaf)
                recursive_range_find_leaf(*m_leaf_id_to_leaf.at(nodeIDs[nodeIDs.size()-1]), lower, upper, result);
            else
                recursive_range_find(*m_node_id_to_node.at(nodeIDs[nodeIDs.size()-1]), lower, upper, curr_depth+1, result);
        }
    }

    void recursive_range_find_leaf(leaf_type& curr_leaf, key_type& lower, key_type& upper, std::vector<value_type>& result) {
        load(curr_leaf);
        std::vector<value_type> buffer_items = curr_leaf.get_buffer_items();

        if (buffer_items[0].first > upper)
            return;

        auto lower_it = std::lower_bound(buffer_items.begin(), buffer_items.end(), value_type(lower, dummy_datum()),
                                         [](const value_type& val1, const value_type& val2)->bool {return val1.first < val2.first;});
        auto upper_it = std::lower_bound(buffer_items.begin(), buffer_items.end(), value_type(upper, dummy_datum()),
                                         [](const value_type& val1, const value_type& val2)->bool {return val1.first < val2.first;});
        if (upper_it != buffer_items.end())
            upper_it++;

        result.insert(result.end(), lower_it, upper_it);
    }


    std::pair<data_type, bool> recursive_find(node_type& curr_node, key_type& key, int curr_depth) {
        /*
         * Pseudocode of function:
         *
         * if current node's buffer contains key:
         *      return (datum associated with the key, true)
         *
         * if the root is the only node:
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
            return std::pair<data_type, bool> (dummy_datum(), false);
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
        if (curr_depth == m_depth - 1) {
            auto it = m_leaf_id_to_leaf.find(child_id);
            assert(it != m_leaf_id_to_leaf.end());
            leaf_type& child = *(it->second);
            return leaf_find(child, key);
        }
        // Child is inner node
        else
            return recursive_find(*m_node_id_to_node.at(child_id), key, curr_depth+1);
    }

    std::pair<data_type, bool> leaf_find(leaf_type& curr_leaf, key_type& key) {
        load(curr_leaf);

        // Search in buffer
        std::pair<data_type, bool> maybe_datum_and_found_in_buffer = curr_leaf.buffer_find(key);
        // If found
        if (maybe_datum_and_found_in_buffer.second)
            return maybe_datum_and_found_in_buffer;
        else
            return std::pair<data_type, bool> (dummy_datum(), false);
    }

};

}

template <typename KeyType,
        typename DataType,
        size_t RawBlockSize,
        size_t RawMemoryPoolSize,
        typename AllocStr = foxxll::default_alloc_strategy
>
using ftree = fractal_tree::fractal_tree<KeyType, DataType, RawBlockSize, RawMemoryPoolSize, AllocStr>;

}

#endif //EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_H