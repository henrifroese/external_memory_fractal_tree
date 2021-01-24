The [STXXL](https://www.stxxl.org/) is a C++ library for external-memory algorithms and data structures. Most implemented data structures expose the same interface as ones from the C++ Standard Template Library. This document serves as a very quick introduction to the main aspects and is adopted from the turorials on the STXXL's website.

# Overview
To facilitate external-memory computation, we need a way to load and store blocks of data from/to disk (block management layer). In order to do that, we need to write/read files from/to disk (asynchronous I/O layer).

## AIO layer

The asynchronous I/O (AIO) layer sits on top of the OS and handles interactions with it for different platforms, providing asynchronous reads and writes to files through `stxxl::file`.
The `stxxl::request_ptr`-objects track asynchronous read and write requests. Ex.:

```cpp
// Callback functor for asynchronous file requests.
struct my_handler {
    void operator () (stxxl::request_ptr req_ptr) {
        std::cout << "Request " << *req_ptr << " completed" << std::endl;
    }
};
// 1 MB buffer to read into
char* my_buffer = new char[1024*1024];



// Open file object
stxxl::syscall_file myfile("./storage", stxxl::file::RDONLY);
// Read the first MB of the opened file, starting with offset 0.
// Set the functor from above as callback
// (will be called as soon as the async. request is finished)
stxxl::request_ptr my_read_request = myfile.aread(mybuffer, 0, 1024*1024, my_handler());

// overlapped with async. read
do_something();
// wait until reading is done
my_read_request->wait();
// have fun
do_something_with_the_data(my_buffer);
delete[] my_buffer;
```

## Block Management layer

The block management layer sits on top of the AIO layer to provide an interface for programming external memory algorithms and data structures. As such algorithms and data structures usually "think" in terms of blocks of data, interacting with the block management layer allows for intuitive implementations.

After seeing the code interacting with the AIO layer above, one can have a rough idea of how blocks could be implemented. The singleton _stxxl::block\_manager_ is responsible for (de-)allocating external memory space on disk. It essentially views the external memory as one big AIO file. When requesting an external memory block from the block manager, the manager returns an appropriate `stxxl::BID` (block identifier) that stores the physical location of the block, i.e. disk and offset.

A single block is an instance of `stxxl::typed_block`. For example, `stxxl::typed_block<4096, int>` is a block of 4kB size that can hold integers.

A simplified "workflow" interacting with the block management layer is then::
```cpp
// Define appropriate `stxxl::typed_block`
using block_type = stxxl::typed_block<4096,int>;
// Get the block manager
stxxl::block_manager* bm = stxxl::block_manager::get_instance();
// Allocate internal memory block with _new_
block_type* im_block = new block_type;
// Write to the internal memory block (note: no I/O has happened yet)
*(im_block->begin()) = 99;
// Get BID for external memory segment to write to
block_type::bid_type bid = block_type::bid_type {};
bm->new_block(stxxl::default_alloc_strategy(), bid);
// Write to external memory (I/O happening)
stxxl::request_ptr req = im_block->write(bid);
req->wait();
// do stuff
// Read into internal memory again (internal memory block from earlier
// might have been used for other blocks) (I/O happening)
im_block->read(bid)->wait();
// do stuff again
delete im_block;
```

Of course there's lots more stuff the block management layer can do, but this should be enough to get started with the STXXL.
