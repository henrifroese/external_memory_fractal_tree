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
        )
    };
    // This is how the data of the inner nodes will be stored in a block.
    struct node_block {
        std::array<value_type, max_num_values_in_node>       values;
        std::array<int,        max_num_values_in_node+1>     nodeIDs;
        std::array<value_type, max_num_buffer_items_in_node> buffer;
    };
    using block_type = node_block;


private:
    const int m_id;
    const bid_type m_bid;
    int m_num_buffer_items = 0;
    int m_num_values = 0;
    block_type* m_block = NULL;

public:
    explicit node(int ID, bid_type BID) : m_id(ID), m_bid(BID) {};
};


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
    using block_type = leaf_block;


private:
    const int m_id;
    const bid_type m_bid;
    int m_num_buffer_items = 0;
    block_type* m_block = NULL;

public:
    explicit leaf(int ID, bid_type BID) : m_id(ID), m_bid(BID) {};
};


}

}
#endif //EXTERNAL_MEMORY_FRACTAL_TREE_NODE_H