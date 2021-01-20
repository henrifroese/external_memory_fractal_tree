//
// Created by henri on 19.01.21.
//

/*
 * TestCache.cpp
 *
 * Copyright (C) 2020 Hung Tran <hung@ae.cs.uni-frankfurt.de>
 */

#include "../include/fractal_tree/fractal_tree.h"
#include "stxxl.h"
#include <fstream>
#include <algorithm>
#include <random>
#include <tuple>

class foxxll_timer {
private:
    const std::string label;
    foxxll::stats* stats;
    const foxxll::stats_data stats_begin;
public:
    explicit foxxll_timer(std::string label_) : label(std::move(label_)), stats(foxxll::stats::get_instance()), stats_begin(*stats) { };
    ~foxxll_timer() = default;

    foxxll::stats_data get_data() {
        return foxxll::stats_data(*stats) - stats_begin;
    }

    void show_data() {
        std::cout << label << ": " << std::endl << (foxxll::stats_data(*stats) - stats_begin) << std::endl;
    }
};

class tree_benchmark {
public:
    struct experiment {
        const int N;
        const int BTREE_WRITES;
        const int BTREE_READS;
        const int FTREE_WRITES;
        const int FTREE_READS;
        const double BTREE_SECONDS;
        const double FTREE_SECONDS;
    };

private:
    const std::string dir;
    const std::string operation;
    const int cachesize;
    const std::string strategy;
    std::vector<experiment> experiments;
public:
    tree_benchmark(std::string dir_, std::string operation_, int cachesize_, std::string strategy_)
            : dir(std::move(dir_)), operation(std::move(operation_)), cachesize(cachesize_), strategy(std::move(strategy_)) {};

    void add_experiment(const int N, const double BTREE_SECONDS, const int BTREE_READS, const int BTREE_WRITES, const double FTREE_SECONDS, const int FTREE_READS, const int FTREE_WRITES) {
        experiments.push_back(experiment {N, BTREE_WRITES, BTREE_READS, FTREE_WRITES, FTREE_READS, BTREE_SECONDS, FTREE_SECONDS });
    }

    void to_csv() {
        std::string filename = dir + "/" + "benchmark_" + operation + "_" + "cachesize" + std::to_string(cachesize) + "_" + "strategy" + strategy + ".csv";
        std::cout << "Exporting to: " << filename << std::endl;
        std::ofstream file;
        file.open(filename);

        if (!file)
            std::cerr << "Error: couldn't open file";

        // Write column names
        file << "N,BTREE_SECONDS,BTREE_WRITES,BTREE_READS,FTREE_SECONDS,FTREE_WRITES,FTREE_READS" << std::endl;
        //  Write rows
        for (auto exp : experiments) {
            file << std::to_string(exp.N) << "," \
                 << std::to_string(exp.BTREE_SECONDS) << "," \
                 << std::to_string(exp.BTREE_WRITES) << "," \
                 << std::to_string(exp.BTREE_READS) << "," \
                 << std::to_string(exp.FTREE_SECONDS) << "," \
                 << std::to_string(exp.FTREE_WRITES) << "," \
                 << std::to_string(exp.FTREE_READS) << "," \
                 << std::endl;
        }

        file.close();
    }
};

using key_type = int;
using data_type = int;
using value_type = std::pair<key_type, data_type >;
constexpr unsigned RawBlockSize = 4096;
struct ComparatorGreater
{
    bool operator () (const int & a, const int & b) const
    { return a > b; }
    static int max_value()
    { return std::numeric_limits<int>::min(); }
};


template<unsigned CacheSize>
std::tuple<double, int, int> benchmark_ftree_insert(std::vector<value_type> values_to_insert) {
    constexpr unsigned RawMemoryPoolSize = CacheSize;

    using ftree_type = stxxl::ftree<key_type, data_type, RawBlockSize, RawMemoryPoolSize>;

    stxxl::ftree<int, int, RawBlockSize, RawMemoryPoolSize> f;

    foxxll_timer custom_timer("FTREE");

    for (auto val : values_to_insert)
        f.insert(val);

    foxxll::stats_data stats_data = custom_timer.get_data();
    custom_timer.show_data();

    // Make sure insertion worked (outside of timing)
    for (auto  val : values_to_insert) {
        std::pair<data_type, bool> found = f.find(val.first);
        assert(found.first == val.second);
        assert(found.second);
    }

    return std::tuple<double, int, int> { stats_data.get_elapsed_time(), stats_data.get_read_count(), stats_data.get_write_count() };

}

template<unsigned CacheSize>
std::tuple<double, int, int> benchmark_btree_insert(std::vector<value_type> values_to_insert) {

    constexpr unsigned RawMemoryPoolSize = CacheSize;

    // template parameter <KeyType, DataType, CompareType, RawNodeSize, RawLeafSize, PDAllocStrategy (optional)>
    using btree_type = stxxl::map<key_type, data_type , ComparatorGreater, RawBlockSize, RawBlockSize>;
    // constructor map(node_cache_size_in_bytes, leaf_cache_size_in_bytes) to create map object named my_map
    btree_type b(RawMemoryPoolSize/2, RawMemoryPoolSize/2);


    foxxll_timer custom_timer("BTREE");

    for (auto val : values_to_insert)
        b.insert(val);

    foxxll::stats_data stats_data = custom_timer.get_data();
    custom_timer.show_data();

    return std::tuple<double, int, int> { stats_data.get_elapsed_time(), stats_data.get_read_count(), stats_data.get_write_count() };
}

template<unsigned CacheSize>
std::tuple<double, int, int> benchmark_ftree_search(std::vector<value_type> values_to_insert_and_search) {
    constexpr unsigned RawMemoryPoolSize = CacheSize;

    using ftree_type = stxxl::ftree<key_type, data_type, RawBlockSize, RawMemoryPoolSize>;

    stxxl::ftree<int, int, RawBlockSize, RawMemoryPoolSize> f;

    for (auto val : values_to_insert_and_search)
        f.insert(val);

    foxxll_timer custom_timer("FTREE");

    for (auto  val : values_to_insert_and_search) {
        f.find(val.first);
    }

    foxxll::stats_data stats_data = custom_timer.get_data();
    custom_timer.show_data();


    return std::tuple<double, int, int> { stats_data.get_elapsed_time(), stats_data.get_read_count(), stats_data.get_write_count() };

}

template<unsigned CacheSize>
std::tuple<double, int, int> benchmark_btree_search(std::vector<value_type> values_to_insert_and_search) {

    constexpr unsigned RawMemoryPoolSize = CacheSize;

    // template parameter <KeyType, DataType, CompareType, RawNodeSize, RawLeafSize, PDAllocStrategy (optional)>
    using btree_type = stxxl::map<key_type, data_type , ComparatorGreater, RawBlockSize, RawBlockSize>;
    // constructor map(node_cache_size_in_bytes, leaf_cache_size_in_bytes) to create map object named my_map
    btree_type b(RawMemoryPoolSize/2, RawMemoryPoolSize/2);

    for (auto val : values_to_insert_and_search)
        b.insert(val);

    foxxll_timer custom_timer("BTREE");

    for (auto val : values_to_insert_and_search)
        b.find(val.first);

    foxxll::stats_data stats_data = custom_timer.get_data();
    custom_timer.show_data();

    return std::tuple<double, int, int> { stats_data.get_elapsed_time(), stats_data.get_read_count(), stats_data.get_write_count() };
}
// ------------------------- BENCHMARKS -------------------------

// Benchmark 1: sequential insertion

void benchmark_1() {
    constexpr unsigned int cachesize = 8 * 4096;
    tree_benchmark b = tree_benchmark("/home/henri/Desktop", "insert", cachesize, "sequential");

    // Have 32kB cache. Insert 32kB to 4 mB
    for (int N=8 * 4096; N <= 1024 * 4096; N = 2*N) {
        // do experiment, get writes and reads for btree and ftree.
        int values_to_insert = N / sizeof(value_type);
        std::vector<value_type> to_insert {};
        to_insert.reserve(values_to_insert);
        for (int i=0; i<values_to_insert; i++)
            to_insert.emplace_back(i,i);

        std::tuple<double, int, int> seconds_reads_writes_ftree = benchmark_ftree_insert<cachesize>(to_insert);
        std::tuple<double, int, int> seconds_reads_writes_btree = benchmark_btree_insert<cachesize>(to_insert);
        // add experiment to benchmark object.
        b.add_experiment(N,
                         std::get<0>(seconds_reads_writes_btree),
                         std::get<1>(seconds_reads_writes_btree),
                         std::get<2>(seconds_reads_writes_btree),
                         std::get<0>(seconds_reads_writes_ftree),
                         std::get<1>(seconds_reads_writes_ftree),
                         std::get<2>(seconds_reads_writes_ftree));
    }

    // export to csv.
    b.to_csv();
}


// Benchmark 2: random insertion

void benchmark_2() {
    constexpr unsigned int cachesize = 8 * 4096;
    tree_benchmark b = tree_benchmark("/home/henri/Desktop", "insert", cachesize, "random");

    // Have 32kB cache. Insert 32kB to 4 mB
    for (int N=8 * 4096; N <= 1024 * 4096; N = 2*N) {
        int values_to_insert = N / sizeof(value_type);
        // do experiment, get writes and reads for btree and ftree.
        std::vector<value_type> to_insert {};
        to_insert.reserve(values_to_insert);
        for (int i=0; i<values_to_insert; i++)
            to_insert.emplace_back(i,i);

        auto rng = std::default_random_engine { 42 };
        std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

        std::tuple<double, int, int> seconds_reads_writes_ftree = benchmark_ftree_insert<cachesize>(to_insert);
        std::tuple<double, int, int> seconds_reads_writes_btree = benchmark_btree_insert<cachesize>(to_insert);
        // add experiment to benchmark object.
        b.add_experiment(N,
                         std::get<0>(seconds_reads_writes_btree),
                         std::get<1>(seconds_reads_writes_btree),
                         std::get<2>(seconds_reads_writes_btree),
                         std::get<0>(seconds_reads_writes_ftree),
                         std::get<1>(seconds_reads_writes_ftree),
                         std::get<2>(seconds_reads_writes_ftree));
    }

    // export to csv.
    b.to_csv();
}

// Benchmark 3: sequential searching

void benchmark_3() {
    constexpr unsigned int cachesize = 8 * 4096;
    tree_benchmark b = tree_benchmark("/home/henri/Desktop", "search", cachesize, "sequential");

    // Have 32kB cache. Insert 32kB to 4 mB
    for (int N=8 * 4096; N <= 1024 * 4096; N = 2*N) {
        // do experiment, get writes and reads for btree and ftree.
        int values_to_insert = N / sizeof(value_type);
        std::vector<value_type> to_insert {};
        to_insert.reserve(values_to_insert);
        for (int i=0; i<values_to_insert; i++)
            to_insert.emplace_back(i,i);

        std::tuple<double, int, int> seconds_reads_writes_ftree = benchmark_ftree_search<cachesize>(to_insert);
        std::tuple<double, int, int> seconds_reads_writes_btree = benchmark_btree_search<cachesize>(to_insert);
        // add experiment to benchmark object.
        b.add_experiment(N,
                         std::get<0>(seconds_reads_writes_btree),
                         std::get<1>(seconds_reads_writes_btree),
                         std::get<2>(seconds_reads_writes_btree),
                         std::get<0>(seconds_reads_writes_ftree),
                         std::get<1>(seconds_reads_writes_ftree),
                         std::get<2>(seconds_reads_writes_ftree));
    }

    // export to csv.
    b.to_csv();
}


// Benchmark 4: random searching

void benchmark_4() {
    constexpr unsigned int cachesize = 8 * 4096;
    tree_benchmark b = tree_benchmark("/home/henri/Desktop", "search", cachesize, "random");

    // Have 32kB cache. Insert 32kB to 4 mB
    for (int N=8 * 4096; N <= 1024 * 4096; N = 2*N) {
        // do experiment, get writes and reads for btree and ftree.
        int values_to_insert = N / sizeof(value_type);
        std::vector<value_type> to_insert {};
        to_insert.reserve(values_to_insert);
        for (int i=0; i<values_to_insert; i++)
            to_insert.emplace_back(i,i);

        auto rng = std::default_random_engine { 42 };
        std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

        std::tuple<double, int, int> seconds_reads_writes_ftree = benchmark_ftree_search<cachesize>(to_insert);
        std::tuple<double, int, int> seconds_reads_writes_btree = benchmark_btree_search<cachesize>(to_insert);
        // add experiment to benchmark object.
        b.add_experiment(N,
                         std::get<0>(seconds_reads_writes_btree),
                         std::get<1>(seconds_reads_writes_btree),
                         std::get<2>(seconds_reads_writes_btree),
                         std::get<0>(seconds_reads_writes_ftree),
                         std::get<1>(seconds_reads_writes_ftree),
                         std::get<2>(seconds_reads_writes_ftree));
    }

    // export to csv.
    b.to_csv();
}

// Random insertion test bigger stuff

template<unsigned long long int CacheSize>
std::tuple<double, int, int> benchmark_ftree_insert2(unsigned long long int N) {

    unsigned long long int num_insertions = N / (sizeof(unsigned long long int) + sizeof(unsigned int));
    std::cout << "Inserting "  << num_insertions << " values. In GB: " << N / 1024 / 1024 / 1024 << std::endl;
    constexpr unsigned long long int RawMemoryPoolSize = CacheSize;

    using ftree_type = stxxl::ftree<unsigned long long int, unsigned int, RawBlockSize, RawMemoryPoolSize>;
    ftree_type f;

    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<unsigned long long int> d(0, 10*num_insertions); // define the range

    foxxll_timer custom_timer("FTREE");

    for (unsigned long long int i = 0; i < num_insertions; i++) {
        if (i % 1000000 == 0) {
            std::cout << i << std::endl;
        }
        f.insert(std::pair<unsigned  long long int, unsigned int> (d(gen), 0));
    }

    foxxll::stats_data stats_data = custom_timer.get_data();
    custom_timer.show_data();

    return std::tuple<double, int, int> { stats_data.get_elapsed_time(), stats_data.get_read_count(), stats_data.get_write_count() };

}

void benchmark_test() {
    // 4 GB cache
    constexpr unsigned long long int cachesize = 0x100000000;
    tree_benchmark b = tree_benchmark("/home/henri/Desktop", "insert", cachesize, "random");

    // Have 4 GB cache. Insert 6 GB
    for (unsigned long long int N=0x180000000; N <= 0x180000000; N = 2*N) {
        // do experiment, get writes and reads for btree and ftree.

        std::tuple<double, int, int> seconds_reads_writes_ftree = benchmark_ftree_insert2<cachesize>(N);
        /*
        std::tuple<double, int, int> seconds_reads_writes_btree = benchmark_ftree_insert2<cachesize>(N);
        // add experiment to benchmark object.
        b.add_experiment(N,
                         std::get<0>(seconds_reads_writes_btree),
                         std::get<1>(seconds_reads_writes_btree),
                         std::get<2>(seconds_reads_writes_btree),
                         std::get<0>(seconds_reads_writes_ftree),
                         std::get<1>(seconds_reads_writes_ftree),
                         std::get<2>(seconds_reads_writes_ftree));
        */
    }

    // export to csv.
    //b.to_csv();
}


int main() {
    //benchmark_1();
    //benchmark_2();
    //benchmark_3();
    //benchmark_4();
    benchmark_test();

    return 0;
}
