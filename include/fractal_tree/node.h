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

    void add_to_buffer(std::vector<value_type>& values) {

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