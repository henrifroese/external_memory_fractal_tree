//
// Created by henri on 15.12.20.
//

#ifndef EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_CACHE_H
#define EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_CACHE_H

#include <tlx/logger.hpp>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <foxxll/io/request_operations.hpp>

namespace stxxl {

namespace fractal_tree {


template<typename BlockType, typename BidType, typename BidHash, unsigned NumBlocksInCache>
class fractal_tree_cache {

    using block_type = BlockType;
    using bid_type = BidType;
    using bid_hash = BidHash;

    enum {
        max_num_blocks_in_cache = NumBlocksInCache
    };

    using bid_block_pair_type = std::pair<bid_type, block_type*>;
    using cache_list_iterator_type = typename std::list<bid_block_pair_type>::iterator;

private:
    std::list<block_type*> m_unused_blocks;
    std::list<bid_block_pair_type> m_cache_list;

    std::unordered_map<bid_type, cache_list_iterator_type, bid_hash> m_cache_map;
    std::unordered_set<bid_type, bid_hash>& m_dirty_bids;

public:
    explicit fractal_tree_cache(std::unordered_set<bid_type, bid_hash>& dirty_bids) : m_dirty_bids(dirty_bids) {
        for (size_t i = 0; i < max_num_blocks_in_cache; i++) {
            auto* block = new block_type;
            // Valgrind warns that we're writing to uninitialized bytes
            // when the struct we use inside the block_type does not
            // fill a full block, so we explicitly set the whole region to
            // 0 here to prevent this
            // (this is the same as setting the FOXXLL_WITH_VALGRIND
            // flag to true in typed_block.hpp)
            memset(block, 0, sizeof(block_type));
            m_unused_blocks.push_back(block);
        }
    }

    ~fractal_tree_cache() {
        for (block_type* block : m_unused_blocks)
            delete block;

        // Delete blocks in cache.
        for (bid_block_pair_type pair : m_cache_list) {
            block_type* block = pair.second;
            delete block;
        }
    }

    // Evict least recently used item.
    void evict() {
        // Get least recently used bid and block.
        bid_block_pair_type lru_item = m_cache_list.back();
        bid_type lru_bid = lru_item.first;
        kick(lru_bid);
    }

    // Load data from a bid into memory.
    // Return the in-memory block with the data.
    block_type* load(const bid_type& bid) {
        auto it = m_cache_map.find(bid);

        // If not in cache ...
        if (it == m_cache_map.end()) {
            // If there are no free blocks, evict an item.
            if (m_unused_blocks.empty())
                evict();
            assert(!m_unused_blocks.empty());

            // Take unused block and load data into it.
            block_type* new_block = m_unused_blocks.back();
            m_unused_blocks.pop_back();
            foxxll::request_ptr req = new_block->read(bid);

            // Insert pair (bid, new_block) into cache list and map
            m_cache_list.push_front(bid_block_pair_type(bid, new_block));
            m_cache_map[bid] = m_cache_list.begin();

            req->wait();
            return new_block;

        // If in cache ...
        } else {
            // Move to front as item was most recently used
            m_cache_list.splice(m_cache_list.begin(), m_cache_list, it->second);
            // Return the block that's in the cache list
            return it->second->second;
        }
    }

    void kick(const bid_type& bid) {
        if (is_cached(bid)) {
            cache_list_iterator_type list_it = m_cache_map.find(bid)->second;
            assert(list_it != m_cache_list.end());
            block_type* block = list_it->second;

            // If necessary, write to external memory.
            if (m_dirty_bids.find(bid) != m_dirty_bids.end()) {
                block->write(bid)->wait();
                m_dirty_bids.erase(bid);
            }
            // Delete entry from cache.
            m_cache_map.erase(bid);
            // Add block back to unused blocks.
            m_unused_blocks.push_back(block);
            m_cache_list.erase(list_it);
        }
    }

    int num_unused_blocks() const {
        return m_unused_blocks.size();
    }

    int num_cached_blocks() const {
        return m_cache_list.size();
    }

    bool is_cached(const bid_type& bid) const {
        return m_cache_map.find(bid) != m_cache_map.end();
    }

    bool is_dirty(const bid_type& bid) const {
        return m_dirty_bids.find(bid) != m_dirty_bids.end();
    }

};

}

}

#endif //EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_CACHE_H
