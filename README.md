# External Memory Fractal Tree

This repository houses the (header-only) implementation of an [external-memory fractal tree](https://en.wikipedia.org/wiki/Fractal_tree_index)
using the C++ [STXXL library](https://stxxl.org/tags/master).

A fractal tree (a variant of the buffered repository tree) is a tree data structure similar to a [B-Tree](https://en.wikipedia.org/wiki/B-tree). Each node occupies one block of memory of size B, and has up to sqrt(B) keys
that function just like those in a B-Tree. However, each node additionally fills its block up with up with a buffer of size
B-sqrt(B). On insertion, new key-datum-pairs are quickly inserted into the root node buffer (and later pushed down to the buffers in the 
next level when a buffer is full), speeding up insertions by a factor 1/sqrt(B)
compared to a B-Tree. Fractal Tree searches are a factor of 2 slower than B-Tree searches.

## Performance
With a Block Size of B, N items in the tree, and for range-search X items in the result, fractal trees
and b-trees need the following number of _disk accesses_ (I/Os):

| Tree Type  | Insertion  | Search  | Range-Search  |
|---|---|---|---|
| Fractal Tree  | ![\frac{1}{\sqrt{B}}\log_B (\frac{N}{B})](https://latex.codecogs.com/gif.latex?%5Cfrac%7B1%7D%7B%5Csqrt%7BB%7D%7D%5Clog_B%20%28%5Cfrac%7BN%7D%7BB%7D%29)  | ![2\log_B (\frac{N}{B})](https://latex.codecogs.com/gif.latex?2%5Clog_B%20%28%5Cfrac%7BN%7D%7BB%7D%29)  | ![2\log_B(\frac{N}{B}) + \frac{X}{B}](https://latex.codecogs.com/gif.latex?2%5Clog_B%28%5Cfrac%7BN%7D%7BB%7D%29%20&plus;%20%5Cfrac%7BX%7D%7BB%7D)  |
| B-Tree  | ![\log_B(\frac{N}{B})](https://latex.codecogs.com/gif.latex?%5Clog_B%28%5Cfrac%7BN%7D%7BB%7D%29)  | ![\log_B(\frac{N}{B})](https://latex.codecogs.com/gif.latex?%5Clog_B%28%5Cfrac%7BN%7D%7BB%7D%29)  | ![\log_B(\frac{N}{B}) + \frac{X}{B}](https://latex.codecogs.com/gif.latex?%5Clog_B%28%5Cfrac%7BN%7D%7BB%7D%29%20&plus;%20%5Cfrac%7BX%7D%7BB%7D)  |

Here are some results of measurements I ran (for details, see [the report](external_memory_fractal_tree_REPORT.pdf)):

<p float="left">
  <img src="/benchmarks/Random_Search.png" width="32%" />
  <img src="/benchmarks/Random_Insertion.png" width="32%" /> 
  <img src="/benchmarks/Range Search (amortized over 5 queries).png" width="32%" />
</p>

## Example Usage
See the [run-fractal-tree.cpp](run-fractal-tree.cpp) file:
```cpp
#include "include/fractal_tree/fractal_tree.h"


int main()
{
    using key_type = unsigned int;
    using data_type = unsigned int;
    using value_type = std::pair<key_type, data_type>;
    constexpr unsigned block_size = 2u << 12u; // 4kB blocks
    constexpr unsigned cache_size = 2u << 15u; // 32kB cache

    using ftree_type = stxxl::ftree<key_type, data_type, block_size, cache_size>;
    ftree_type f;

    // insert 1MB of data
    for (key_type k = 0; k < (2u << 20u) / sizeof(value_type); k++) {
        f.insert(value_type(k, 2*k));
    }

    // find datum of given key
    std::pair<data_type, bool> datum_and_found = f.find(1);
    assert(datum_and_found.second);
    assert(datum_and_found.first == 2);

    // find values in key range [100, 1000]
    std::vector<value_type> range_values = f.range_find(100, 1000);
    std::vector<value_type> correct_range_values {};
    for (key_type k = 100; k <= 1000; k++) {
        correct_range_values.emplace_back(k, 2*k);
    }
    assert(range_values == correct_range_values);


    return 0;
}

```
## Building & Using
1. clone the repo
2. cd into the repo
3. run `git submodule init`
4. run `git submodule update --init --recursive`

## Details
More implementation details, an introduction to external memory trees, and benchmarks can be found [in this report](external_memory_fractal_tree_REPORT.pdf).
