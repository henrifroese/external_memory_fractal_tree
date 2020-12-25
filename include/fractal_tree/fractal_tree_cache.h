//
// Created by henri on 15.12.20.
//

#ifndef EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_CACHE_H
#define EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_CACHE_H


#include <list>
#include <unordered_map>
#include <unordered_set>
#include <foxxll/io/request_operations.hpp>


template<typename BlockType, typename BidType, unsigned NumBlocksInCache>
class fractal_tree_cache {

    using block_type = BlockType;
    using bid_type = BidType;

    enum {
        max_num_blocks_in_cache = NumBlocksInCache
    };

    using bid_block_pair_type = std::pair<bid_type, block_type*>;
    using cache_list_iterator_type = typename std::list<bid_block_pair_type>::iterator;

public:
    explicit fractal_tree_cache(std::unordered_set<bid_type>& dirty_bids) : m_dirty_bids(dirty_bids) {
        for (size_t i = 0; i < max_num_blocks_in_cache; i++)
            m_unused_blocks.push_back(new block_type);
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

    void evict() {
        // Get least recently used bid and block.
        auto lru_it = m_cache_list.back();
        bid_type lru_bid = lru_it->first;
        block_type lru_block = lru_it->second;

        // If necessary, write to external memory.
        if (m_dirty_bids.find(lru_bid) != m_dirty_bids.end())
            lru_block->write(lru_bid)->wait();

        m_dirty_bids.erase(lru_bid);
        // Delete entry from cache.
        m_cache_map.erase(lru_bid);
        m_cache_list.pop_back();
    }

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


private:
    std::list<block_type*> m_unused_blocks;
    std::list<bid_block_pair_type> m_cache_list;

    struct bid_hash {
        size_t operator () (const bid_type& bid) const {
            size_t result = foxxll::longhash1(bid.offset + reinterpret_cast<uint64_t>(bid.storage));
            return result;
        }
    };
    std::unordered_map<bid_type, cache_list_iterator_type, bid_hash> m_cache_map;
    std::unordered_set<bid_type, bid_hash> m_dirty_bids;
};



#endif //EXTERNAL_MEMORY_FRACTAL_TREE_FRACTAL_TREE_CACHE_H
