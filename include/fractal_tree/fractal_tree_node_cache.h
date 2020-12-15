//
// Created by henri on 08.12.20.
//

#ifndef EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_NODE_CACHE_H
#define EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_NODE_CACHE_H

#include <limits>
#include <unordered_map>
#include <tlx/define/likely.hpp>
#include <tlx/logger.hpp>
#include <foxxll/mng/typed_block.hpp>
#include <foxxll/mng/read_write_pool.hpp>
#include <foxxll/mng/block_alloc_strategy.hpp>
#include <stxxl/bits/defines.h>
#include <stxxl/bits/containers/pager.h>

#include "node.h"

#define STRING(VALUE) #VALUE
#define LOG_VALUE(VALUE) TLX_LOG1 << STRING(VALUE) << ":\t" << VALUE;


namespace stxxl {

    template <typename FractalTreeType>
    class fractal_tree_node_cache {
        static constexpr bool debug = true;

    public:
        //! identifier types
        using bid_type                  = typename FractalTreeType::bid_type;

        //! foxxll types
        using alloc_strategy_type       = typename FractalTreeType::AllocStr;

        // own types
        using node_type                 = typename FractalTreeType::node_type;
        using leaf_type                 = typename FractalTreeType::leaf_type;

        using node_block_type           = typename FractalTreeType::node_block_type;
        using leaf_block_type           = typename FractalTreeType::leaf_block_type;

        using node_block_pool_type      = foxxll::read_write_pool<node_block_type>;
        using leaf_block_pool_type      = foxxll::read_write_pool<leaf_block_type>;

        //! stxxl types
        using pager_type                = stxxl::lru_pager<>;

        struct bid_hash {
            size_t operator() (const bid_type& bid) const {
                size_t result = foxxll::longhash1(bid.offset + reinterpret_cast<intptr_t>(bid.storage));
                return result;
            }
        };

        // own constants
        enum {
            raw_block_size = FractalTreeType::RawBlockSize,
            raw_memory_pool_size = FractalTreeType::RawMemoryPoolSize,
        };

        enum {
            // TODO check validity
            // Dividing by 4 as we have equally-sized pools for the nodes and the leaves.
            init_num_prefetch_blocks = (raw_memory_pool_size / 4) / raw_block_size,
            init_num_write_blocks = (raw_memory_pool_size / 4) / raw_block_size
        };

        // TODO pool with multiple disks
        explicit fractal_tree_node_cache(const size_t n_blocks)
                : n_cacheable_blocks(n_blocks),
                  m_bm(foxxll::block_manager::get_instance())
        {
            // TODO readd multiple disks
            const size_t disks = 1; // (D < 1) ? foxxll::config::get_instance()->disks_number() : static_cast<size_t>(D);

            m_node_blocks_pool = new node_block_pool_type(init_num_prefetch_blocks, init_num_write_blocks);
            m_leaf_blocks_pool = new leaf_block_pool_type(init_num_prefetch_blocks, init_num_write_blocks);

            LOG_VALUE(n_cacheable_blocks);
        }

        //! non-copyable: delete copy-constructor
        fractal_tree_node_cache(const fractal_tree_node_cache&) = delete;

        //! non-copyable: delete assignment operator
        fractal_tree_node_cache& operator = (const fractal_tree_node_cache&) = delete;

        // allocate new block in memory file
        void alloc_new_block(bid_type& new_bid)
        {
            ++n_created;
            m_bm->new_block(m_alloc_strategy, new_bid);
            TLX_LOG << "[allocated] @ " << new_bid;
        }

        // retrieve block from pool
        node_block_type* get_node_block()
        {
            ++n_nodes_served;
            node_block_type* pool_ptr = m_node_blocks_pool->steal();
            return pool_ptr;
        }
        leaf_block_type* get_leaf_block()
        {
            ++n_nodes_served;
            leaf_block_type* pool_ptr = m_leaf_blocks_pool->steal();
            return pool_ptr;
        }

        // writes the provided block at pointer's position to specified block identified by the block-identifier
        void write_block(node_block_type*& node_block, bid_type& bid)
        {
            m_node_blocks_pool->write(node_block, bid);
            TLX_LOG << "[written] @ " << bid;
        }
        void write_block(leaf_block_type*& leaf_block, bid_type& bid)
        {
            m_leaf_blocks_pool->write(leaf_block, bid);
            TLX_LOG << "[written] @ " << bid;
        }

        // TODO add prefetching possibilities
        // reads the block specified by the block-identifier at the pointer's position
        void read_block(node_block_type*& node_block, const bid_type& bid)
        {
            foxxll::request_ptr req = m_node_blocks_pool->read(node_block, bid);
            req->wait();
            TLX_LOG << "[read] @ " << bid;
        }
        void read_block(leaf_block_type*& leaf_block, const bid_type& bid)
        {
            foxxll::request_ptr req = m_leaf_blocks_pool->read(leaf_block, bid);
            req->wait();
            TLX_LOG << "[read] @ " << bid;
        }

        void read(node_type& node) {
            node_block_type* in_memory_block = get_node_block();
            read_block(in_memory_block, node.m_bid);
            node.m_block = in_memory_block->begin();
        }

        void read(leaf_type& leaf) {
            node_block_type* in_memory_block = get_node_block();
            read_block(in_memory_block, leaf.m_bid);
            leaf.m_block = in_memory_block->begin();
        }

        void write(node_type& node) {
            write_block(node.m_block, node.m_bid);
        }

        void write(leaf_type& leaf) {
            write_block(leaf.m_block, leaf.m_bid);
        }

/*
        void load_node(node);

        void prefetch_node(node);

        void write_node(node);

        void read_node(node);*/

    private:
        // number of overall held blocks in all pools
        const size_t n_cacheable_blocks;

        // number of blocks that have been served throughout the life-time
        size_t n_nodes_served { 0 };
        size_t n_leaves_served { 0 };

        // number of blocks that have been allocated
        size_t n_created { 0 };

        // block manager to allocate/create new blocks
        foxxll::block_manager* m_bm;

        // foxxll allocation strategy used for the pools
        alloc_strategy_type m_alloc_strategy;

        // foxxll pools
        node_block_pool_type* m_node_blocks_pool;
        leaf_block_pool_type* m_leaf_blocks_pool;
    };

}




#endif //EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_NODE_CACHE_H
