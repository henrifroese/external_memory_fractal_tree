//
// Created by henri on 08.12.20.
//


#include <iostream>
#include <foxxll/mng/typed_block.hpp>
#include <foxxll/mng/read_write_pool.hpp>
#include <foxxll/mng/block_alloc_strategy.hpp>
#include <stxxl/bits/containers/pager.h>


struct leaf_block {
    std::array<int, 1024> buffer;
};

int main () {

    const unsigned int RawBlockSize = 4096;
    using bid_type = foxxll::BID<RawBlockSize>;
    using leaf_block_type = foxxll::typed_block<RawBlockSize, leaf_block>;
    using leaf_block_pool_type = foxxll::read_write_pool<leaf_block_type>;

    foxxll::block_manager* bm = foxxll::block_manager::get_instance();


    // Example without pool -------------------------------------------------------------------------

    // -------------------Writing--------------------

    // Make mock data
    leaf_block my_data;
    for (int i=0; i<1024; i++)
        my_data.buffer[i] = i;

    // Get in-memory block, and external-memory block with bid
    leaf_block_type* IM_block = new leaf_block_type;
    bid_type my_bid;
    bm->new_block(foxxll::default_alloc_strategy(), my_bid);

    // Write data to in-memory block
    *(IM_block->begin()) = my_data;

    // Write in-memory block to EM
    foxxll::request_ptr r = IM_block->write(my_bid);
    r->wait();

    delete IM_block;


    // -------------------Reading--------------------

    // Get in-memory block and read the external-memory data into it.
    leaf_block_type* IM_block2 = new leaf_block_type;
    foxxll::request_ptr r2 = IM_block2->read(my_bid);
    r2->wait();
    leaf_block my_data_again = *(IM_block2->begin());

    for (auto i : my_data_again.buffer) {
        std::cout << i << std::endl;
    }

    delete IM_block2;

    bm->delete_block(my_bid);




    // Example with pool -------------------------------------------------------------------------------
    leaf_block_pool_type* leaf_blocks_pool = new leaf_block_pool_type(1, 1);

    // -------------------Writing--------------------

    // Get in-memory block, and external-memory block with bid
    leaf_block_type* IM_block3 = leaf_blocks_pool->steal();                                   // get_leaf_block()
    bid_type my_bid2;                                                                         // same
    bm->new_block(foxxll::default_alloc_strategy(), my_bid2);                        // alloc_new_block(bid_type& new_bid)

    // Write data to in-memory block
    *(IM_block3->begin()) = my_data;

    // Write in-memory block to EM
    leaf_blocks_pool->write(IM_block3, my_bid2);                                            // write_block(leaf_block_type*& leaf_block, bid_type& bid)
    assert(IM_block3 == nullptr);

    // -------------------Reading--------------------

    // Get in-memory block and read the external-memory data into it.
    leaf_block_type* IM_block4 = leaf_blocks_pool->steal();                                    // get_leaf_block()
    foxxll::request_ptr r3 = leaf_blocks_pool->read(IM_block4, my_bid2);                    // read_block(leaf_block_type*& leaf_block, const bid_type& bid)
    r3->wait();
    leaf_block my_data_again2 = *(IM_block4->begin());

    for (auto i : my_data_again2.buffer) {
        std::cout << i << std::endl;
    }

    delete IM_block4;

    delete leaf_blocks_pool;

    return 0;
}