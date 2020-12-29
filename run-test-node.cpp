/*
 * run-fractal-tree.cpp
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran     <hung@ae.cs.uni-frankfurt.de>
 */


#include <tlx/die.hpp>
#include "include/fractal_tree/fractal_tree.h"
#include "include/fractal_tree/fractal_tree_cache.h"

using key_type = int;
using data_type = int;
using value_type = std::pair<key_type, data_type>;

constexpr unsigned RawBlockSize = 4096;

using bid_type = foxxll::BID<RawBlockSize>;

constexpr unsigned num_items = RawBlockSize / sizeof(value_type);

struct block {
    std::array<value_type, num_items> A {};
    std::array<char, RawBlockSize - num_items * sizeof(value_type)> padding {};
};

using block_type = foxxll::typed_block<RawBlockSize, block>;
foxxll::block_manager* bm;

struct bid_hash {
    size_t operator () (const bid_type& bid) const {
        size_t result = foxxll::longhash1(bid.offset + reinterpret_cast<uint64_t>(bid.storage));
        return result;
    }
};

// Tests for node and leaf parameters -----------------------------------
void test_node_parameters() {
    //TODO
}

void test_leaf_parameters() {
    //TODO
}

// Tests for free functions in node.h -----------------------------------
void test_free_function_merge_into() {
    //TODO
}

// Tests for node class -------------------------------------------------

void test_node_basic() {
    //TODO
}

// Tests for node class: buffer -----------------------------------------

// Tests for node class: buffer setters ---------------------------------

void test_node_buffer_setters_basic() {
    //TODO
}

void test_node_buffer_setters_add_to_buffer() {
    //TODO
}

// Tests for node class: buffer getters ---------------------------------

void test_node_buffer_getters_basic() {
    //TODO
}

void test_node_buffer_getters_index_of_upper_bound_of_buffer() {
    //TODO
}

void test_node_buffer_getters_get_buffer_items_less_than() {
    //TODO
}

void test_node_buffer_getters_get_buffer_items_greater_equal_than() {
    //TODO
}

void test_node_buffer_getters_get_mid_buffer_item() {
    //TODO
}

void test_node_buffer_getters_buffer_find() {
    //TODO
}

// Tests for node class: values -----------------------------------------

// Tests for node class: values setters ---------------------------------

void test_node_values_setters_basic() {
    //TODO
}

void test_node_values_setters_update_duplicate_values() {
    //TODO
}

void test_node_values_setters_add_to_values() {
    //TODO
}

// Tests for node class: values getters ---------------------------------

void test_node_values_getters_basic() {
    //TODO
}

void test_node_values_getters_values_find() {
    //TODO
}

// Tests for leaf class -------------------------------------------------

void test_leaf_basic() {
    //TODO
}

// Tests for leaf class: buffer -----------------------------------------

// Tests for node class: buffer setters ---------------------------------

void test_leaf_buffer_setters_add_to_buffer() {
    //TODO
}

// Tests for node class: buffer getters ---------------------------------

void test_leaf_buffer_getters_buffer_find() {
    //TODO
}

int main()
{

    return 0;
}