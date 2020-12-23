/*
 * node.h
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran <hung@ae.cs.uni-frankfurt.de>
 */

#ifndef EXTERNAL_MEMORY_FRACTAL_TREE_NODE_H
#define EXTERNAL_MEMORY_FRACTAL_TREE_NODE_H


#include <foxxll/mng/typed_block.hpp>


namespace stxxl {

namespace fractal_tree {


// Free functions to use for nodes and leaves.

// Merge sorted vectors, and take from new_values in case of duplicates
template<typename value_type>
std::vector<value_type> merge_into(const std::vector<value_type>& new_values, const std::vector<value_type>& current_values) {
    std::vector<value_type> result;

    auto it_new_values = new_values.begin();
    auto it_current_values = current_values.begin();

    while ((it_new_values != new_values.end()) && (it_current_values != current_values.end())) {
        // Compare by key
        if (it_new_values->first < it_current_values->first) {
            result.push_back(*it_new_values);
            it_new_values++;
            continue;
        }
        if (it_new_values->first > it_current_values->first) {
            result.push_back(*it_current_values);
            it_current_values++;
            continue;
        }
        // Equal keys -> take new value, discard value from buffer
        result.push_back(*it_new_values);
        it_new_values++;
        it_current_values++;
    }
    // Insert remaining elements
    if (it_new_values != new_values.end()) {
        std::copy(it_new_values, new_values.end(), std::back_inserter(result));
    }
    if (it_current_values != current_values.end()) {
        std::copy(it_current_values, current_values.end(), std::back_inserter(result));
    }

    return result;
}


template<typename KeyType,
     typename DataType,
     unsigned RawBlockSize>
class node final {
public:
    // Basic type declarations
    using key_type = KeyType;
    using data_type = DataType;
    using value_type = std::pair<const key_type, data_type>;
    using self_type = node<KeyType, DataType, RawBlockSize>;
    using bid_type = foxxll::BID<RawBlockSize>;

    // Set up sizes and types for the blocks used to store inner nodes' data in external memory.
    enum {
        max_num_values_in_node = (int) sqrt((double) RawBlockSize / (sizeof(KeyType) + sizeof(DataType))),
        max_num_buffer_items_in_node = (int) (
                (RawBlockSize - ((sizeof(KeyType) + sizeof(DataType) + sizeof(int)) * max_num_values_in_node)) /
                (sizeof(KeyType) + sizeof(DataType))
        ),
        buffer_mid = (max_num_buffer_items_in_node - 1) / 2,
        values_mid = (max_num_values_in_node - 1) / 2
    };

    // This is how the data of the inner nodes will be stored in a block.
    struct node_block {
        std::array<value_type, max_num_values_in_node>       values;
        std::array<int,        max_num_values_in_node+1>     nodeIDs;
        std::array<value_type, max_num_buffer_items_in_node> buffer;
    };
    using block_type = foxxll::typed_block<RawBlockSize, node_block>;
    using buffer_iterator_type = typename std::array<value_type, max_num_buffer_items_in_node>::iterator;
    using values_iterator_type = typename std::array<value_type, max_num_values_in_node>::iterator;

    static data_type dummy_datum = data_type();

private:
    const int m_id;
    const bid_type m_bid;

private:
    int m_num_buffer_items = 0;
    int m_num_values = 0;
    block_type* m_block = nullptr;

    std::array<const value_type, max_num_values_in_node>*       m_values  = nullptr;
    std::array<const int,        max_num_values_in_node+1>*     m_nodeIDs = nullptr;
    std::array<const value_type, max_num_buffer_items_in_node>* m_buffer  = nullptr;

public:
    explicit node(int ID, bid_type BID) : m_id(ID), m_bid(BID) {};

    const bid_type& get_bid() const {
        return m_bid;
    }

    int get_id() const {
        return m_id;
    }

    int get_max_buffer_size() const {
        return max_num_buffer_items_in_node;
    }

    bool buffer_full() {
        return m_num_buffer_items == max_num_buffer_items_in_node;
    }

    block_type* get_block() {
        return m_block;
    }

    void set_block(block_type*& block) {
        m_block = block;
        m_values = &(m_block->begin()->values);
        m_nodeIDs = &(m_block->begin()->nodeIDs);
        m_buffer = &(m_block->begin()->buffer);
    }

    std::vector<value_type> get_left_half_buffer_items() const {
        return std::vector<value_type>(m_buffer->begin(), m_buffer->begin()+buffer_mid);
    }

    std::vector<value_type> get_right_half_buffer_items() const {
        return std::vector<value_type>(m_buffer->begin()+buffer_mid+1, m_buffer->end());
    }

    value_type get_mid_buffer_item() const {
        return m_buffer->at(buffer_mid);
    }

    void clear_buffer() {
        m_num_buffer_items = 0;
    }

    // Add the new values to the buffer. In case of duplicate keys,
    // take the data from new_values.
    void add_to_buffer(std::vector<value_type>& new_values) {
        /*
         * When adding new values to the node's buffer, we might
         * have duplicate keys in the new values and the current values,
         * and duplicate keys in the new values and the current buffer items.
         *
         * Pseudocode:
         * 1. Check for duplicate keys in new_values and current values.
         *    Use the new data for all duplicates.
         * 2. For the remaining new_values, merge them with the current
         *    buffer items, and again use the new value for all duplicates.
         */

        bool is_sorted = std::is_sorted(new_values.begin(), new_values.end(),
                                        [] (value_type val1, value_type val2)->bool { return val1.first < val2.first; });
        assert(is_sorted);

        // 1.
        // Check for and replace duplicate keys in new_values and current values.
        new_values = update_duplicate_values(new_values);

        // 2.
        std::vector<value_type> buffer_values = std::vector<value_type>(m_buffer->begin(), m_buffer->begin()+m_num_buffer_items);

        // Merge, and take from values in case of duplicates
        assert(new_values.size() + buffer_values.size() <= max_num_buffer_items_in_node);
        std::vector<value_type> new_buffer_values = merge_into<value_type>(new_values, buffer_values);

        // Replace buffer with new buffer values
        std::copy(new_buffer_values.begin(), new_buffer_values.end(), m_buffer->begin());
        m_num_buffer_items = new_buffer_values.size();
    }

    // Given new_values that should be inserted to the buffer, replace duplicate keys
    // in values and new_values with the new data. Return the remaining non-duplicate
    // values.
    std::vector<value_type> update_duplicate_values(const std::vector<value_type>& new_values) {
        std::vector<value_type> remaining_new_values;

        auto it_new_values = new_values.begin();
        auto it_current_values = m_values->begin();

        while ((it_new_values != new_values.end()) && (it_current_values != m_values->end())) {
            // Compare by key
            if (it_new_values->first == it_current_values->first) {
                // Duplicate found -> take data from new_values
                it_current_values->second = it_new_values->second;
                it_current_values++;
                it_new_values++;
            } else {
                // No duplicate -> still want to insert the new value into the buffer
                remaining_new_values.push_back(*it_current_values);
            }
        }

        return remaining_new_values;
    }

    std::pair<data_type, bool> buffer_find(const key_type& key) const {
        // Binary search for key
        auto it = std::lower_bound(
                m_buffer->begin(),
                m_buffer->begin() + m_num_buffer_items,
                key,
                [](const value_type& val1, const value_type& val2)->bool {return val1.first < val2.first;}
                );

        // std::lower_bound finds first key that's >= what we look for, or the end
        bool found = (it != m_buffer->begin() + m_num_buffer_items) && (it->first == key);

        if (found)
            return std::pair<data_type, bool>(it->second, false);
        else
            return std::pair<data_type, bool>(dummy_datum, false);
    }


    // Add value to the node's values, and add the corresponding children to
    // the node's nodeIDs
    void add_to_values(value_type value, int left_child_id, int right_child_id) {
        /*
         * Pseudocode:
         * 1. insert value into values of node
         * 2. get index i where value was inserted
         * 3. move nodeIDs from i onwards one to the right
         * 4. insert left child id in free space at index i
         * 5. overwrite id at i+1 with right child id
         */
        // 1.
        // Binary search for position to insert
        auto insert_position_it = std::lower_bound(
                m_values->begin(),
                m_values->begin() + m_num_values,
                value.first,
                [](const value_type& val1, const value_type& val2)->bool {return val1.first < val2.first;}
        );
        // Shift all values [position to insert, last position] one to the right
        // to make space for the new value.
        for (auto it = m_values->begin() + m_num_buffer_items; it != insert_position_it; it--) {
            *it = *(it-1);
        }
        // Insert new value
        *insert_position_it = value;

        // 2.
        int insert_position_index = std::distance(m_values->begin(), insert_position_it);
        auto nodeID_insert_position_it = m_nodeIDs->begin() + insert_position_index;

        // 3.
        // Shift all nodeIDs [position to insert, last position] one to the right
        for (auto it = m_nodeIDs->begin() + m_num_values + 1; it != nodeID_insert_position_it; it--) {
            *it = *(it-1);
        }
        // 4. + 5.
        *nodeID_insert_position_it = left_child_id;
        *(nodeID_insert_position_it+1) = right_child_id;

        m_num_values++;
    }

    // Find key in values array. Return type:
    // < <datum of key if found else dummy_datum, id of child to go to>, bool whether key was found >
    std::pair<std::pair<data_type, int>, bool> values_find(const key_type& key) const {
        // Binary search for key
        auto it = std::lower_bound(
                m_values->begin(),
                m_values->begin() + m_num_values,
                key,
                [](const value_type& val1, const value_type& val2)->bool {return val1.first < val2.first;}
        );

        // std::lower_bound finds first key that's >= what we look for, or the end
        bool found = (it != m_values->begin() + m_num_values) && (it->first == key);

        if (found)
            return std::pair<std::pair<data_type, int>, bool>(
                    std::pair<data_type, int>(it->second,0),
                    true
                    );
        else {
            // First key that's >= what we're looking for is
            // at index i -> want to go to i'th child next.
            int index_of_first_key_greater_equal_key = it - m_values->begin();
            int id_of_child_to_go_to = m_nodeIDs[index_of_first_key_greater_equal_key];

            return std::pair<std::pair<data_type, int>, bool>(
                    std::pair<data_type, int>(dummy_datum,id_of_child_to_go_to),
                    false
            );
        }

    }
};

template<typename KeyType,
        typename DataType,
        unsigned RawBlockSize>
bool operator == (const node<KeyType, DataType, RawBlockSize>& node1, const node<KeyType, DataType, RawBlockSize>& node2) {
    return node1.get_id() == node2.get_id();
}

template<typename KeyType,
        typename DataType,
        unsigned RawBlockSize>
bool operator != (const node<KeyType, DataType, RawBlockSize>& node1, const node<KeyType, DataType, RawBlockSize>& node2) {
    return !(node1.get_id() == node2.get_id());
}


template<typename KeyType,
        typename DataType,
        unsigned RawBlockSize>
class leaf final {
    // Type declarations
    using key_type = KeyType;
    using data_type = DataType;
    using value_type = std::pair<const key_type, data_type>;
    using self_type = leaf<KeyType, DataType, RawBlockSize>;
    using bid_type = foxxll::BID<RawBlockSize>;

    // Set up sizes and types for the blocks used to store inner nodes' data in external memory.
    enum {
        max_num_buffer_items_in_leaf = (int) (RawBlockSize / (sizeof(KeyType) + sizeof(DataType)))
    };
    // This is how the data of the leaves will be stored in a block.
    struct leaf_block {
        std::array<std::pair<KeyType,DataType>, max_num_buffer_items_in_leaf> buffer;
    };
    using block_type = foxxll::typed_block<RawBlockSize, leaf_block>;

    static data_type dummy_datum = data_type();

private:
    const int m_id;
    const bid_type m_bid;
    int m_num_buffer_items = 0;
    block_type* m_block = NULL;

    std::array<value_type, max_num_buffer_items_in_leaf>* m_buffer  = nullptr;

public:
    explicit leaf(int ID, bid_type BID) : m_id(ID), m_bid(BID) {};

    const bid_type& get_bid() const {
        return m_bid;
    }

    int get_id() const {
        return m_id;
    }

    void set_block(block_type*& block) {
        m_block = block;
        m_buffer = m_block->begin()->buffer;
    }

    // Add the new values to the buffer. In case of duplicate keys,
    // take the data from new_values.
    void add_to_buffer(std::vector<value_type>& new_values) {

        bool is_sorted = std::is_sorted(new_values.begin(), new_values.end(),
                                        [] (value_type val1, value_type val2)->bool { return val1.first < val2.first; });
        assert(is_sorted);

        std::vector<value_type> buffer_values = std::vector<value_type>(m_buffer->begin(), m_buffer->begin()+m_num_buffer_items);

        // Merge, and take from values in case of duplicates
        assert(new_values.size() + buffer_values.size() <= max_num_buffer_items_in_leaf);
        std::vector<value_type> new_buffer_values = merge_into<value_type>(new_values, buffer_values);

        // Replace buffer with new buffer values
        std::copy(new_buffer_values.begin(), new_buffer_values.end(), m_buffer->begin());
        m_num_buffer_items = new_buffer_values.size();
    }

    std::pair<data_type, bool> buffer_find(const key_type& key) const {
        // Binary search for key
        auto it = std::lower_bound(
                m_buffer->begin(),
                m_buffer->begin() + m_num_buffer_items,
                key,
                [](const value_type& val1, const value_type& val2)->bool {return val1.first < val2.first;}
        );

        // std::lower_bound finds first key that's >= what we look for, or the end
        bool found = (it != m_buffer->begin() + m_num_buffer_items) && (it->first == key);

        if (found)
            return std::pair<data_type, bool>(it->second, false);
        else
            return std::pair<data_type, bool>(dummy_datum, false);
    }

};


}

}
#endif //EXTERNAL_MEMORY_FRACTAL_TREE_NODE_H