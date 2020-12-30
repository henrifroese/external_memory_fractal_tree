/*
 * run-fractal-tree.cpp
 *
 * Copyright (C) 2020 Henri Froese
 *                    Hung Tran     <hung@ae.cs.uni-frankfurt.de>
 */

#include <tlx/die.hpp>
#include "include/fractal_tree/node.h"
#include "include/fractal_tree/fractal_tree_cache.h"
#include "include/fractal_tree/fractal_tree.h"

using key_type = int;
using data_type = int;
using value_type = std::pair<key_type, data_type>;

constexpr unsigned RawBlockSize = 4096;

using bid_type = foxxll::BID<RawBlockSize>;

constexpr unsigned num_items = RawBlockSize / sizeof(value_type);

foxxll::block_manager* bm;


// Tests for node and leaf parameters -----------------------------------
void test_node_parameters() {
    // This tests for the static assertions in the node class
    // for different key and datum types
    // (-> if this compiles, the tests are passing).
    stxxl::fractal_tree::node<int, int, RawBlockSize> n(0, bid_type());
    stxxl::fractal_tree::node<double, int, RawBlockSize> n2(0, bid_type());
    stxxl::fractal_tree::node<int, double, RawBlockSize> n3(0, bid_type());
    stxxl::fractal_tree::node<double, double, RawBlockSize> n4(0, bid_type());
    stxxl::fractal_tree::node<char, int, RawBlockSize> n5(0, bid_type());
    stxxl::fractal_tree::node<std::pair<char, char>, int, RawBlockSize> n6(0, bid_type());
    stxxl::fractal_tree::node<std::pair<double, char>, bool, RawBlockSize> n7(0, bid_type());
    stxxl::fractal_tree::node<std::array<double, 10>, bool, RawBlockSize> n8(0, bid_type());
}

void test_leaf_parameters() {
    // This tests for the static assertions in the leaf class
    // for different key and datum types
    // (-> if this compiles, the tests are passing).
    stxxl::fractal_tree::leaf<int, int, RawBlockSize> n(0, bid_type());
    stxxl::fractal_tree::leaf<double, int, RawBlockSize> n2(0, bid_type());
    stxxl::fractal_tree::leaf<int, double, RawBlockSize> n3(0, bid_type());
    stxxl::fractal_tree::leaf<double, double, RawBlockSize> n4(0, bid_type());
    stxxl::fractal_tree::leaf<char, int, RawBlockSize> n5(0, bid_type());
    stxxl::fractal_tree::leaf<std::pair<char, char>, int, RawBlockSize> n6(0, bid_type());
    stxxl::fractal_tree::leaf<std::pair<double, char>, bool, RawBlockSize> n7(0, bid_type());
    stxxl::fractal_tree::leaf<std::array<double, 10>, bool, RawBlockSize> n8(0, bid_type());
}

// Tests for free functions in node.h -----------------------------------
void test_free_function_merge_into() {
    std::vector<value_type> V1, V2, R;

    V1 = { {1,1}, {2,1} };
    V2 = { {1,2}, {2,2}, {3,2} };
    R  = { {1,1}, {2,1}, {3,2} };
    assert(stxxl::fractal_tree::merge_into<value_type>(V1, V2) == R);

    V1 = { {3,1}, {4,1} };
    V2 = { {1,2}, {2,2}, {3,2}, {5,2} };
    R  = { {1,2}, {2,2}, {3,1}, {4,1}, {5, 2} };
    assert(stxxl::fractal_tree::merge_into<value_type>(V1, V2) == R);

    V1 = { };
    V2 = { {1,2}, {2,2}, {3,2} };
    R  = { {1,2}, {2,2}, {3,2} };
    assert(stxxl::fractal_tree::merge_into<value_type>(V1, V2) == R);

    V1 = { {1,1}, {2,1}, {3,1} };
    V2 = { };
    R  = { {1,1}, {2,1}, {3,1} };
    assert(stxxl::fractal_tree::merge_into<value_type>(V1, V2) == R);
}

// Tests for node class -------------------------------------------------
using node_type = stxxl::fractal_tree::node<key_type, data_type, RawBlockSize>;

void test_node_basic() {
    bid_type bid {};
    node_type n(10, bid);

    assert(n.get_bid() == bid);
    assert(n.get_id() == 10);
}

// Tests for node class: buffer -----------------------------------------

// Tests for node class: buffer setters ---------------------------------

void test_node_buffer_setters_basic() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;

    n.set_block(block);

    assert(n.buffer_empty());

    std::vector<value_type> V;
    std::vector<value_type> W;
    // Fill up fully
    V.clear();
    for (int i=0; i<n.max_buffer_size(); i++)
        V.emplace_back(i, i);
    W = V;
    n.set_buffer(V);

    assert(n.buffer_full());
    assert(n.get_buffer_items() == W);

    // Fill up half
    V.clear();
    for (int i=0; i<n.max_buffer_size()/2; i++)
        V.emplace_back(i*2, i*2);
    W = V;
    n.set_buffer(V);

    assert(n.num_items_in_buffer() == n.max_buffer_size()/2);
    assert(n.get_buffer_items() == W);

    // Fill up empty
    V.clear();
    W = V;
    n.set_buffer(V);

    assert(n.buffer_empty());
    assert(n.get_buffer_items() == W);

    delete block;
}

void test_node_buffer_setters_add_to_buffer() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;

    n.set_block(block);

    std::vector<value_type> values = { {3,1}, {5,1} };
    std::vector<int> nodeIDs = { 1, 2, 3 };

    // Case: buffer currently empty
    n.set_values_and_nodeIDs(values, nodeIDs);

    std::vector<value_type> new_buffer_items =
            { {1,2}, {3,2}, {4,2}, {6,2} };
    n.add_to_buffer(new_buffer_items);
    std::vector<value_type> empty {};
    n.add_to_buffer(empty);

    std::vector<value_type> values_after_adding_to_buffer =
            { {3,2}, {5,1} };
    std::vector<value_type> buffer_after_adding_to_buffer =
            { {1,2}, {4,2}, {6,2} };

    assert(n.get_buffer_items() == buffer_after_adding_to_buffer);
    assert(n.get_values() == values_after_adding_to_buffer);

    // Case: buffer currently not empty
    new_buffer_items = { {4,3}, {5,3}, {6,3} };

    n.add_to_buffer(new_buffer_items);

    values_after_adding_to_buffer = { {3,2}, {5,3} };
    buffer_after_adding_to_buffer = { {1,2}, {4,3}, {6,3} };

    assert(n.get_buffer_items() == buffer_after_adding_to_buffer);
    assert(n.get_values() == values_after_adding_to_buffer);

    delete block;
}

// Tests for node class: buffer getters ---------------------------------

void test_node_buffer_getters_basic() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;
    n.set_block(block);

    // Test with full buffer
    std::vector<value_type> V;
    std::vector<value_type> W;
    for (int i=0; i<n.max_buffer_size(); i++)
        V.emplace_back(i, i);
    W = V;
    n.set_buffer(V);
    assert(n.num_items_in_buffer() == n.max_buffer_size());

    // get_buffer_item(index)
    for (int i=0; i<n.num_items_in_buffer(); i++) {
        assert(n.get_buffer_item(i) == W.at(i));
    }
    // get_buffer_items()
    assert(n.get_buffer_items() == W);
    // get_buffer_items(low, high)
    assert(n.get_buffer_items(0, n.num_items_in_buffer()) == std::vector<value_type>(W.begin(), W.end()));
    assert(n.get_buffer_items(0, n.num_items_in_buffer()-1) == std::vector<value_type>(W.begin(), W.end()-1));
    assert(n.get_buffer_items(1, n.num_items_in_buffer()) == std::vector<value_type>(W.begin()+1, W.end()));
    assert(n.get_buffer_items(1, n.num_items_in_buffer()-1) == std::vector<value_type>(W.begin()+1, W.end()-1));
    assert(n.get_buffer_items(0, 0) == std::vector<value_type>(W.begin(), W.begin()));
    assert(n.get_buffer_items(n.num_items_in_buffer(), n.num_items_in_buffer()) == std::vector<value_type>(W.end(), W.end()));
    assert(n.get_buffer_items(4, 7) == std::vector<value_type>(W.begin()+4, W.begin()+7));
    assert(n.get_buffer_items(2, 3) == std::vector<value_type>(W.begin()+2, W.begin()+3));

    // Test with half full buffer
    V.clear();
    W.clear();
    for (int i=0; i<n.max_buffer_size()/2; i++)
        V.emplace_back(i, i);
    W = V;
    n.set_buffer(V);
    assert(n.num_items_in_buffer() == n.max_buffer_size()/2);

    // get_buffer_item(index)
    for (int i=0; i<n.num_items_in_buffer(); i++) {
        assert(n.get_buffer_item(i) == W.at(i));
    }
    // get_buffer_items()
    assert(n.get_buffer_items() == W);
    // get_buffer_items(low, high)
    assert(n.get_buffer_items(0, n.num_items_in_buffer()) == std::vector<value_type>(W.begin(), W.end()));
    assert(n.get_buffer_items(0, n.num_items_in_buffer()-1) == std::vector<value_type>(W.begin(), W.end()-1));
    assert(n.get_buffer_items(1, n.num_items_in_buffer()) == std::vector<value_type>(W.begin()+1, W.end()));
    assert(n.get_buffer_items(1, n.num_items_in_buffer()-1) == std::vector<value_type>(W.begin()+1, W.end()-1));
    assert(n.get_buffer_items(0, 0) == std::vector<value_type>(W.begin(), W.begin()));
    assert(n.get_buffer_items(n.num_items_in_buffer(), n.num_items_in_buffer()) == std::vector<value_type>(W.end(), W.end()));
    assert(n.get_buffer_items(4, 7) == std::vector<value_type>(W.begin()+4, W.begin()+7));
    assert(n.get_buffer_items(2, 3) == std::vector<value_type>(W.begin()+2, W.begin()+3));

    // Test with empty buffer
    V.clear();
    W.clear();
    n.set_buffer(V);
    assert(n.num_items_in_buffer() == 0);

    // get_buffer_items()
    assert(n.get_buffer_items() == W);
    // get_buffer_items(low, high)
    assert(n.get_buffer_items(0, 0) == std::vector<value_type>(W.begin(), W.begin()));

    delete block;
}

void test_node_buffer_getters_index_of_upper_bound_of_buffer() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;
    n.set_block(block);

    std::vector<value_type> values = { {3,1}, {5,1}, {8,1} };
    std::vector<int> nodeIDs = { 10, 11, 12, 13 };
    std::vector<value_type> buffer_items =
            { {1,2}, {2,2}, {4,2}, {6,2}, {7,2}, {9,2}, {10,2} };

    n.set_values_and_nodeIDs(values, nodeIDs);
    n.set_buffer(buffer_items);

    assert(n.index_of_upper_bound_of_buffer(0) == 2);
    assert(n.index_of_upper_bound_of_buffer(1) == 3);
    assert(n.index_of_upper_bound_of_buffer(2) == 5);
    assert(n.index_of_upper_bound_of_buffer(3) == 7);

    // With empty buffer
    n.clear_buffer();
    assert(n.index_of_upper_bound_of_buffer(0) == 0);
    assert(n.index_of_upper_bound_of_buffer(1) == 0);
    assert(n.index_of_upper_bound_of_buffer(2) == 0);
    assert(n.index_of_upper_bound_of_buffer(3) == 0);

    delete block;
}

void test_node_buffer_getters_get_buffer_items_less_than() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;
    n.set_block(block);

    std::vector<value_type> buffer_items =
            { {1,2}, {2,2}, {4,2}, {6,2}, {7,2}, {9,2}, {10,2} };
    n.set_buffer(buffer_items);

    std::vector<value_type> correct {};
    for (int i=-1; i<15; i++) {
        correct.clear();
        for (auto pair : buffer_items) {
            if (pair.first < i)
                correct.push_back(pair);
        }
        assert(n.get_buffer_items_less_than(value_type(i,0)) == correct);
    }

    // Test with empty buffer
    n.clear_buffer();
    correct.clear();

    for (int i=-1; i<15; i++)
        assert(n.get_buffer_items_less_than(value_type(i,0)) == correct);

    delete block;
}

void test_node_buffer_getters_get_buffer_items_greater_equal_than() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;
    n.set_block(block);

    std::vector<value_type> buffer_items =
            { {1,2}, {2,2}, {4,2}, {6,2}, {7,2}, {9,2}, {10,2} };
    n.set_buffer(buffer_items);

    std::vector<value_type> correct {};
    for (int i=-1; i<15; i++) {
        correct.clear();
        for (auto pair : buffer_items) {
            if (pair.first >= i)
                correct.push_back(pair);
        }
        assert(n.get_buffer_items_greater_equal_than(value_type(i,0)) == correct);
    }

    // Test with empty buffer
    n.clear_buffer();
    correct.clear();

    for (int i=-1; i<15; i++)
        assert(n.get_buffer_items_greater_equal_than(value_type(i,0)) == correct);

    delete block;
}

void test_node_buffer_getters_buffer_find() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;
    n.set_block(block);

    std::vector<value_type> buffer_items =
            { {1,2}, {2,2}, {4,2}, {6,2}, {7,2}, {9,2}, {10,2} };
    n.set_buffer(buffer_items);

    for (int i=-1; i<15; i++) {
        auto found_it =
                std::find_if(
                        buffer_items.begin(),
                        buffer_items.end(),
                        [&](value_type v1)->bool {return v1.first == i;}
                        );

        if (found_it != buffer_items.end())
            assert(n.buffer_find(i) == std::make_pair(found_it->second, true));
        else
            assert(n.buffer_find(i).second == false);
    }

    // Test with empty buffer
    n.clear_buffer();

    for (int i=-1; i<15; i++)
        assert(n.buffer_find(i).second == false);

    delete block;
}

// Tests for node class: values -----------------------------------------

// Tests for node class: values setters ---------------------------------

void test_node_values_setters_basic() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;

    n.set_block(block);

    std::vector<value_type> values{};
    std::vector<int> nodeIDs{};

    // Empty values
    assert(n.num_values() == 0);
    assert(n.num_children() == 0);
    assert(n.get_values() == values);
    assert(n.get_nodeIDs(0, n.num_children()) == nodeIDs);

    // Fill up completely
    for (int i=0; i<n.max_num_values_in_node; i++) {
        values.emplace_back(i, i);
        nodeIDs.push_back(i);
    }
    nodeIDs.push_back(n.max_num_values_in_node);

    n.clear();
    n.set_values_and_nodeIDs(values, nodeIDs);

    assert(n.num_values() == n.max_num_values_in_node);
    assert(n.num_children() == n.max_num_values_in_node + 1);
    assert(n.get_values() == values);
    assert(n.get_nodeIDs(0, n.num_values()+1) == nodeIDs);

    // Fill up half
    values.clear();
    nodeIDs.clear();
    for (int i=0; i<n.max_num_values_in_node/2; i++) {
        values.emplace_back(i, i);
        nodeIDs.push_back(i);
    }
    nodeIDs.push_back(n.max_num_values_in_node/2);

    n.clear();
    n.set_values_and_nodeIDs(values, nodeIDs);

    assert(n.get_values() == values);
    assert(n.get_nodeIDs(0, n.num_values()+1) == nodeIDs);

    delete block;
}

void test_node_values_setters_update_duplicate_values() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;

    n.set_block(block);

    // Case: empty values and empty input
    std::vector<value_type> remaining_new_values = n.update_duplicate_values(std::vector<value_type> {});
    assert(n.get_values().empty());
    assert(remaining_new_values.empty());

    // Case: empty values and non-empty input
    std::vector<value_type> new_buffer_values =
            { {1,2}, {3,2}, {4,2}, {6,2} };
    remaining_new_values = n.update_duplicate_values(new_buffer_values);
    assert(n.get_values().empty());
    assert(remaining_new_values == new_buffer_values);

    // Case: non-empty values and empty input
    std::vector<value_type> values = { {3,1}, {5,1} };
    std::vector<int> nodeIDs = { 1, 2, 3 };

    n.set_values_and_nodeIDs(values, nodeIDs);
    remaining_new_values = n.update_duplicate_values(std::vector<value_type> {});
    assert(n.get_values() == values);
    assert(remaining_new_values.empty());

    // Case: non-empty values and non-empty input
    n.clear();
    n.set_values_and_nodeIDs(values, nodeIDs);
    new_buffer_values = { {1,2}, {3,2}, {4,2}, {6,2} };

    remaining_new_values = n.update_duplicate_values(new_buffer_values);

    std::vector<value_type> values_result =
            { {3,2}, {5,1} };
    std::vector<value_type> remaining_new_values_result =
            { {1,2}, {4,2}, {6,2} };

    assert(n.get_values() == values_result);
    assert(remaining_new_values == remaining_new_values_result);

    delete block;
}

void test_node_values_setters_add_to_values() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;
    n.set_block(block);

    std::vector<value_type> values {};
    std::vector<int> nodeIDs {};
    std::vector<value_type> values_result;
    std::vector<int> nodeIDs_result;

    // Case: empty values
    n.add_to_values(value_type(0,0), 10, 12);
    values_result = { {0,0} };
    nodeIDs_result = { 10, 12 };
    assert(n.get_values() == values_result);
    assert(n.get_nodeIDs(0, n.num_children()) == nodeIDs_result);

    // Case: non-empty values
    n.clear();
    values = { {3,1}, {5,1}, {8,1} };
    nodeIDs = { 10, 11, 12, 13 };
    n.set_values_and_nodeIDs(values, nodeIDs);
    n.add_to_values(value_type(0,0), 1, 2);
    values_result = { {0,0}, {3,1}, {5,1}, {8,1} };
    nodeIDs_result = { 1, 2, 11, 12, 13 };
    assert(n.get_values() == values_result);
    assert(n.get_nodeIDs(0, n.num_children()) == nodeIDs_result);

    n.clear();
    values = { {3,1}, {5,1}, {8,1} };
    nodeIDs = { 10, 11, 12, 13 };
    n.set_values_and_nodeIDs(values, nodeIDs);
    n.add_to_values(value_type(4,0), 1, 2);
    values_result = { {3,1}, {4,0}, {5,1}, {8,1} };
    nodeIDs_result = { 10, 1, 2, 12, 13 };
    assert(n.get_values() == values_result);
    assert(n.get_nodeIDs(0, n.num_children()) == nodeIDs_result);

    n.clear();
    values = { {3,1}, {5,1}, {8,1} };
    nodeIDs = { 10, 11, 12, 13 };
    n.set_values_and_nodeIDs(values, nodeIDs);
    n.add_to_values(value_type(9,0), 1, 2);
    values_result = { {3,1}, {5,1}, {8,1}, {9,0} };
    nodeIDs_result = { 10, 11, 12, 1, 2 };
    assert(n.get_values() == values_result);
    assert(n.get_nodeIDs(0, n.num_children()) == nodeIDs_result);

    delete block;
}

// Tests for node class: values getters ---------------------------------

void test_node_values_getters_basic() {
    //get_values(low, high), get_value, get_nodeIDs(low, high)
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;
    n.set_block(block);

    // Test with full values
    std::vector<value_type> values;
    std::vector<int> nodeIDs;
    std::vector<value_type> values_;
    std::vector<int> nodeIDs_;
    for (int i=0; i<n.max_num_values_in_node; i++)
        values.emplace_back(i, i);
    for (int i=0; i<n.max_num_values_in_node+1; i++)
        nodeIDs.push_back(i);
    values_ = values;
    nodeIDs_ = nodeIDs;
    
    n.set_values_and_nodeIDs(values, nodeIDs);

    // get_value(index)
    for (int i=0; i<n.max_num_values_in_node; i++)
        assert(n.get_value(i) == values_.at(i));
    
    // get_values()
    assert(n.get_values() == values_);

    // get_values(low, high)
    assert(n.get_values(0, n.num_values()) == std::vector<value_type>(values_.begin(), values_.end()));
    assert(n.get_values(0, n.num_values()-1) == std::vector<value_type>(values_.begin(), values_.end()-1));
    assert(n.get_values(1, n.num_values()) == std::vector<value_type>(values_.begin()+1, values_.end()));
    assert(n.get_values(1, n.num_values()-1) == std::vector<value_type>(values_.begin()+1, values_.end()-1));
    assert(n.get_values(0, 0) == std::vector<value_type>(values_.begin(), values_.begin()));
    assert(n.get_values(n.num_values(), n.num_values()) == std::vector<value_type>(values_.end(), values_.end()));
    assert(n.get_values(4, 7) == std::vector<value_type>(values_.begin()+4, values_.begin()+7));
    assert(n.get_values(2, 3) == std::vector<value_type>(values_.begin()+2, values_.begin()+3));
    
    // get_nodeIDs(low, high)
    assert(n.get_nodeIDs(0, n.num_children()) == std::vector<int>(nodeIDs_.begin(), nodeIDs_.end()));
    assert(n.get_nodeIDs(0, n.num_children()-1) == std::vector<int>(nodeIDs_.begin(), nodeIDs_.end()-1));
    assert(n.get_nodeIDs(1, n.num_children()) == std::vector<int>(nodeIDs_.begin()+1, nodeIDs_.end()));
    assert(n.get_nodeIDs(1, n.num_children()-1) == std::vector<int>(nodeIDs_.begin()+1, nodeIDs_.end()-1));
    assert(n.get_nodeIDs(0, 0) == std::vector<int>(nodeIDs_.begin(), nodeIDs_.begin()));
    assert(n.get_nodeIDs(n.num_children(), n.num_children()) == std::vector<int>(nodeIDs_.end(), nodeIDs_.end()));
    assert(n.get_nodeIDs(4, 7) == std::vector<int>(nodeIDs_.begin()+4, nodeIDs_.begin()+7));
    assert(n.get_nodeIDs(2, 3) == std::vector<int>(nodeIDs_.begin()+2, nodeIDs_.begin()+3));

    // Test with empty values
    values.clear();
    values_.clear();
    nodeIDs.clear();
    nodeIDs_.clear();
    n.clear();

    // get_values()
    assert(n.get_values() == values);
    // get_values(low, high)
    assert(n.get_values(0, 0) == std::vector<value_type>(values_.begin(), values_.begin()));
    // get_nodeIDs(low, high)
    assert(n.get_nodeIDs(0, 0) == std::vector<int>(nodeIDs_.begin(), nodeIDs_.begin()));

    delete block;
}

void test_node_values_getters_values_find() {
    node_type n(10, bid_type());
    auto* block = new node_type::block_type;
    n.set_block(block);

    std::vector<value_type> values = { {3,0}, {7,2}, {20,4} };
    std::vector<int> nodeIDs = { 10, 11, 12, 13 };
    n.set_values_and_nodeIDs(values, nodeIDs);

    std::pair<std::pair<data_type, int>, bool> result;

    // Test keys belonging to first child
    for (int i=-1; i<3; i++) {
        result = n.values_find(i);
        assert(result.second == false);
        assert(result.first.second == 10);
    }
    // Test keys belonging to second child
    for (int i=4; i<7; i++) {
        result = n.values_find(i);
        assert(result.second == false);
        assert(result.first.second == 11);
    }
    // Test keys belonging to third child
    for (int i=8; i<20; i++) {
        result = n.values_find(i);
        assert(result.second == false);
        assert(result.first.second == 12);
    }
    // Test keys belonging to fourth child
    for (int i=21; i<30; i++) {
        result = n.values_find(i);
        assert(result.second == false);
        assert(result.first.second == 13);
    }

    // Test exact matches
    assert(n.values_find(3).second == true);
    assert(n.values_find(3).first.first == 0);
    assert(n.values_find(7).second == true);
    assert(n.values_find(7).first.first == 2);
    assert(n.values_find(20).second == true);
    assert(n.values_find(20).first.first == 4);

    delete block;
}

// Tests for leaf class -------------------------------------------------
using leaf_type = stxxl::fractal_tree::leaf<key_type, data_type, RawBlockSize>;

void test_leaf_basic() {
    bid_type bid {};
    leaf_type l(10, bid);

    assert(l.get_bid() == bid);
    assert(l.get_id() == 10);
}

// Tests for leaf class: buffer -----------------------------------------

// Tests for node class: buffer setters ---------------------------------
void test_leaf_buffer_setters_basic() {
    leaf_type n(10, bid_type());
    auto* block = new leaf_type::block_type;

    n.set_block(block);

    assert(n.buffer_empty());

    std::vector<value_type> V;
    std::vector<value_type> W;
    // Fill up fully
    V.clear();
    for (int i=0; i<n.max_buffer_size(); i++)
        V.emplace_back(i, i);
    W = V;
    n.set_buffer(V);

    assert(n.buffer_full());
    assert(n.get_buffer_items() == W);

    // Fill up half
    V.clear();
    for (int i=0; i<n.max_buffer_size()/2; i++)
        V.emplace_back(i*2, i*2);
    W = V;
    n.set_buffer(V);

    assert(n.num_items_in_buffer() == n.max_buffer_size()/2);
    assert(n.get_buffer_items() == W);

    // Fill up empty
    V.clear();
    W = V;
    n.set_buffer(V);

    assert(n.buffer_empty());
    assert(n.get_buffer_items() == W);

    delete block;
}


void test_leaf_buffer_setters_add_to_buffer() {
    leaf_type n(10, bid_type());
    auto* block = new leaf_type::block_type;

    n.set_block(block);

    std::vector<value_type> buffer_after_adding_to_buffer {};
    std::vector<value_type> new_buffer_items {};

    // Case: buffer currently empty
    new_buffer_items = { {1,2}, {3,2}, {4,2}, {6,2} };
    buffer_after_adding_to_buffer = new_buffer_items;
    n.add_to_buffer(new_buffer_items);

    std::vector<value_type> empty {};
    n.add_to_buffer(empty);

    assert(n.get_buffer_items() == buffer_after_adding_to_buffer);

    // Case: buffer currently not empty
    new_buffer_items = { {4,3}, {5,3}, {6,3} };
    buffer_after_adding_to_buffer = { {1,2}, {3,2}, {4,3}, {5,3}, {6,3} };
    n.add_to_buffer(new_buffer_items);

    assert(n.get_buffer_items() == buffer_after_adding_to_buffer);

    delete block;
}

// Tests for node class: buffer getters ---------------------------------

void test_leaf_buffer_getters_buffer_find() {
    leaf_type n(10, bid_type());
    auto* block = new leaf_type::block_type;
    n.set_block(block);

    std::vector<value_type> buffer_items =
            { {1,2}, {2,2}, {4,2}, {6,2}, {7,2}, {9,2}, {10,2} };
    n.set_buffer(buffer_items);

    for (int i=-1; i<15; i++) {
        auto found_it =
                std::find_if(
                        buffer_items.begin(),
                        buffer_items.end(),
                        [&](value_type v1)->bool {return v1.first == i;}
                );

        if (found_it != buffer_items.end())
            assert(n.buffer_find(i) == std::make_pair(found_it->second, true));
        else
            assert(n.buffer_find(i).second == false);
    }

    // Test with empty buffer
    n.clear_buffer();

    for (int i=-1; i<15; i++)
        assert(n.buffer_find(i).second == false);

    delete block;}

int main()
{
    test_free_function_merge_into();

    test_node_parameters();
    test_leaf_parameters();

    test_node_basic();
    test_node_buffer_setters_basic();
    test_node_buffer_setters_add_to_buffer();
    test_node_buffer_getters_basic();
    test_node_buffer_getters_index_of_upper_bound_of_buffer();
    test_node_buffer_getters_get_buffer_items_less_than();
    test_node_buffer_getters_get_buffer_items_greater_equal_than();
    test_node_buffer_getters_buffer_find();
    test_node_values_setters_basic();
    test_node_values_setters_update_duplicate_values();
    test_node_values_setters_add_to_values();
    test_node_values_getters_basic();

    test_leaf_basic();
    test_leaf_buffer_setters_basic();
    test_leaf_buffer_setters_add_to_buffer();
    test_leaf_buffer_getters_buffer_find();

    return 0;
}