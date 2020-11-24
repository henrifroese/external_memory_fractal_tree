/*
 * node.h
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran <hung@ae.cs.uni-frankfurt.de>
 */

#ifndef EXTERNAL_MEMORY_FRACTAL_TREE_NODE_H
#define EXTERNAL_MEMORY_FRACTAL_TREE_NODE_H

namespace stxxl {

// TODO
namespace fractal_tree {

template<typename KeyType,
         typename DataType,
         typename KeyCompareWithMaxType,
         unsigned RawNodeSize>
class node {
    using key_type = KeyType;
    using data_type = DataType;
    using key_compare = KeyCompareWithMaxType;
    using value_type = std::pair<const key_type, data_type>;

public:
    // TODO remove
    node() = default;
};

}

}
#endif //EXTERNAL_MEMORY_FRACTAL_TREE_NODE_H