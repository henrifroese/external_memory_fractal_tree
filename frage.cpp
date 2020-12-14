
#include <iostream>
#include <foxxll/mng/typed_block.hpp>
#include <foxxll/mng/read_write_pool.hpp>
#include <foxxll/mng/block_alloc_strategy.hpp>
#include <stxxl/bits/containers/pager.h>
#include <stxxl/sequence>

/*
Frage:

Ich will den Pool ja eigentlich so benutzen, dass ich immer wenn ich eine Node benutzen will,
erstmal sowas wie node_cache.load_node_data_into_memory(node) mache. Das soll den Block mit ID
node.BID in einen in-memory Block einlesen und dessen Adresse im Pointer node.block speichern,
damit die node damit arbeiten kann. Wenn ich was ändere, will ich dann node_cache.write_node(node) machen.

Unten sind jetzt Beispiele zum Lesen und Schreiben mit dem Pool. Da sieht es jetzt für mich aus,
als müsste ich die in-memory-blöcke, die ich von pool->steal() bekomme, selbst deleten (wenn
ich z.B. delete IM_block1 auskommentiere, sagt mir Valgrind Memcheck dass ich einen memory leak habe).

Das ergibt für mich aber keinen Sinn: Wenn ich den Speicher für die in-memory-Blöcke selbst wieder freigeben
muss, wie kann dann der Pool noch was für mich bringen? Ich dachte, ich lasse mir immer mit pool->steal()
einen in-memory Block geben (was potentiell einen anderen rauskickt), kann dann da einen external-memory Block einlesen, und so benutzen. Wenn
ich die aber immer selbst löschen muss nach Benutzung, dann muss ich mir ja selbst überlegen was ich wann
in-memory habe und rauswerfe, dann scheint der Pool nichts zu bringen.

Ich glaube ich verstehe hier also irgendwie den Pool nicht richtig. 
*/


constexpr size_t size = 512 * 1024;
using value_type = std::array<int, size>;

struct leaf_block {
    value_type buffer;
};

constexpr size_t RawBlockSize = STXXL_DEFAULT_BLOCK_SIZE(leaf_block);
using bid_type = foxxll::BID<RawBlockSize>;
using leaf_block_type = foxxll::typed_block<RawBlockSize, leaf_block>;
using leaf_block_pool_type = foxxll::read_write_pool<leaf_block_type>;

class CacheSimulator {
public:
    CacheSimulator(foxxll::block_manager*& bm, leaf_block_pool_type*& leaf_blocks_pool)
    {
        // Make mock data
        leaf_block my_data;
        for (int i = 0; static_cast<size_t>(i) < size; i++)
            my_data.buffer[i] = i;

        /// Writing

        // Generate new block
        bid_type my_bid;
        bm->new_block(foxxll::default_alloc_strategy(), my_bid);

        std::cout << "initial setup" << std::endl;
        std::cout << "pool::size_write()\t\t" << leaf_blocks_pool->size_write() << std::endl;

        // Get in-memory block, and external-memory block with bid
        auto im_fst_block = leaf_blocks_pool->steal();
        auto im_fst_block_data = im_fst_block->begin();

        // Write data to in-memory block, value by value
        for (auto it = im_fst_block_data->buffer.begin();
             it != im_fst_block_data->buffer.end();
             ++it)
        {
            (*it) = std::distance(im_fst_block_data->buffer.begin(), it);
        }

        // Write in-memory block to pool
        leaf_blocks_pool->write(im_fst_block, my_bid)->wait();

        std::cout << "after writing" << std::endl;
        std::cout << "pool::size_write()\t\t" << leaf_blocks_pool->size_write() << std::endl;

        /// Reading

        // Get in-memory block and read the external-memory data into it.
        leaf_block_type* im_snd_block = leaf_blocks_pool->steal();
        auto read_req = leaf_blocks_pool->read(im_snd_block, my_bid);
        read_req->wait();

        leaf_block my_data_again = *(im_snd_block->begin());

        int j = 0;
        for (auto i : my_data_again.buffer) {
            assert(i == j);
            j++;
        }

        std::cout << "after reading" << std::endl;
        std::cout << "pool::size_write()\t\t" << leaf_blocks_pool->size_write() << std::endl;
    }

    ~CacheSimulator() = default;
};

int main () {
    {
        std::cout << "sizeof(leaf_block)\t" << sizeof(leaf_block) << std::endl;
        std::cout << "sizeof(leaf_block_type)\t" << sizeof(leaf_block_type) << std::endl;
        std::cout << "leaf_block_type::size\t" << leaf_block_type::size << std::endl << std::endl;

        // block manager
        auto bm = foxxll::block_manager::get_instance();

        // read/write pool
        auto pool = new leaf_block_pool_type(1, 4);
        std::cout << "initial setup" << std::endl;
        std::cout << "pool::size_write()\t\t" << pool->size_write() << std::endl;

        // run example
        CacheSimulator simulator(bm, pool);

        //delete bm;
        delete pool;
    }

    return 0;
}
