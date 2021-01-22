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

// Helper class for benchmarking.
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
    const std::string operation;
    const int cachesize;
    const std::string strategy;
    std::vector<experiment> experiments;
public:
    tree_benchmark(std::string operation_, int cachesize_, std::string strategy_)
            : operation(std::move(operation_)), cachesize(cachesize_), strategy(std::move(strategy_)) {};

    void add_experiment(const int N, const double BTREE_SECONDS, const int BTREE_READS, const int BTREE_WRITES, const double FTREE_SECONDS, const int FTREE_READS, const int FTREE_WRITES) {
        experiments.push_back(experiment {N, BTREE_WRITES, BTREE_READS, FTREE_WRITES, FTREE_READS, BTREE_SECONDS, FTREE_SECONDS });
    }

    void to_csv() {
        std::string filename = "./benchmark_" + operation + "_" + "cachesize" + std::to_string(cachesize) + "_" + "strategy" + strategy + ".csv";
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

// ---------------- Helper functions for the benchmarks ----------------------

template<unsigned CacheSize>
std::tuple<double, int, int> benchmark_ftree_insert(std::vector<value_type> values_to_insert) {
    constexpr unsigned RawMemoryPoolSize = CacheSize;

    using ftree_type = stxxl::ftree<key_type, data_type, RawBlockSize, RawMemoryPoolSize>;
    ftree_type f;

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
    ftree_type f;

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

    // Want to force the b-tree to use the const-qualified
    // version of the overloaded find function to prevent
    // unnecessary writes.
    // See https://stackoverflow.com/questions/27315451/
    const auto* b_const = &b;

    for (auto val : values_to_insert_and_search) {
        btree_type::const_iterator x = b_const->find(val.first);
    }

    foxxll::stats_data stats_data = custom_timer.get_data();
    custom_timer.show_data();

    return std::tuple<double, int, int> { stats_data.get_elapsed_time(), stats_data.get_read_count(), stats_data.get_write_count() };
}


template<unsigned CacheSize>
std::tuple<double, int, int> benchmark_ftree_rangesearch(std::vector<value_type> values_to_insert_and_search) {
    constexpr unsigned RawMemoryPoolSize = CacheSize;

    using ftree_type = stxxl::ftree<key_type, data_type, RawBlockSize, RawMemoryPoolSize>;
    ftree_type f;

    for (auto val : values_to_insert_and_search)
        f.insert(val);

    foxxll_timer custom_timer("FTREE");

    f.range_find(0, values_to_insert_and_search.size() - 1);

    foxxll::stats_data stats_data = custom_timer.get_data();
    custom_timer.show_data();


    return std::tuple<double, int, int> { stats_data.get_elapsed_time(), stats_data.get_read_count(), stats_data.get_write_count() };

}

template<unsigned CacheSize>
std::tuple<double, int, int> benchmark_btree_rangesearch(std::vector<value_type> values_to_insert_and_search) {

    constexpr unsigned RawMemoryPoolSize = CacheSize;

    // template parameter <KeyType, DataType, CompareType, RawNodeSize, RawLeafSize, PDAllocStrategy (optional)>
    using btree_type = stxxl::map<key_type, data_type , ComparatorGreater, RawBlockSize, RawBlockSize>;
    // constructor map(node_cache_size_in_bytes, leaf_cache_size_in_bytes) to create map object named my_map
    btree_type b(RawMemoryPoolSize/2, RawMemoryPoolSize/2);

    for (auto val : values_to_insert_and_search)
        b.insert(val);

    // Want to force the b-tree to use the const-qualified
    // version of the overloaded find function to prevent
    // unnecessary writes.
    // See https://stackoverflow.com/questions/27315451/
    const auto* b_const = &b;

    foxxll_timer custom_timer("BTREE");

    // b-tree does not have actual range-search directly,
    // so we use lower bound and then walk
    // along.
    btree_type::const_iterator lower = b_const->lower_bound(0);
    btree_type::const_iterator upper = b_const->upper_bound(values_to_insert_and_search.size()-1);

    while (lower != upper) {
        lower--;
    }

    foxxll::stats_data stats_data = custom_timer.get_data();
    custom_timer.show_data();

    return std::tuple<double, int, int> { stats_data.get_elapsed_time(), stats_data.get_read_count(), stats_data.get_write_count() };
}

template<unsigned CacheSize>
std::tuple<double, int, int> benchmark_ftree_rangesearch_repeated(std::vector<value_type> values_to_insert_and_search) {
    constexpr unsigned RawMemoryPoolSize = CacheSize;

    using ftree_type = stxxl::ftree<key_type, data_type, RawBlockSize, RawMemoryPoolSize>;
    ftree_type f;

    for (auto val : values_to_insert_and_search)
        f.insert(val);

    std::vector<double> elapsed_time {};
    std::vector<int> read_count {};
    std::vector<int> write_count {};
    for (int i=0; i<5; i++) {
        foxxll_timer custom_timer("FTREE");

        f.range_find(0, values_to_insert_and_search.size() - 1);

        foxxll::stats_data stats_data = custom_timer.get_data();

        elapsed_time.push_back(stats_data.get_elapsed_time());
        read_count.push_back(stats_data.get_read_count());
        write_count.push_back(stats_data.get_write_count());
    }
    double average_elapsed_time = std::accumulate(elapsed_time.begin(), elapsed_time.end(), 0.0) / elapsed_time.size();
    int average_reads = std::accumulate(read_count.begin(), read_count.end(), 0LL) / read_count.size();
    int average_writes = std::accumulate(write_count.begin(), write_count.end(), 0LL) / write_count.size();

    return std::tuple<double, int, int> { average_elapsed_time, average_reads, average_writes };
}

template<unsigned CacheSize>
std::tuple<double, int, int> benchmark_btree_rangesearch_repeated(std::vector<value_type> values_to_insert_and_search) {

    constexpr unsigned RawMemoryPoolSize = CacheSize;

    // template parameter <KeyType, DataType, CompareType, RawNodeSize, RawLeafSize, PDAllocStrategy (optional)>
    using btree_type = stxxl::map<key_type, data_type , ComparatorGreater, RawBlockSize, RawBlockSize>;
    // constructor map(node_cache_size_in_bytes, leaf_cache_size_in_bytes) to create map object named my_map
    btree_type b(RawMemoryPoolSize/2, RawMemoryPoolSize/2);

    for (auto val : values_to_insert_and_search)
        b.insert(val);

    // Want to force the b-tree to use the const-qualified
    // version of the overloaded find function to prevent
    // unnecessary writes.
    // See https://stackoverflow.com/questions/27315451/
    const auto* b_const = &b;

    std::vector<double> elapsed_time {};
    std::vector<int> read_count {};
    std::vector<int> write_count {};
    for (int i=0; i<5; i++) {
        foxxll_timer custom_timer("BTREE");

        // b-tree does not have actual range-search directly,
        // so we use lower bound and then walk
        // along.
        btree_type::const_iterator lower = b_const->lower_bound(0);
        btree_type::const_iterator upper = b_const->upper_bound(values_to_insert_and_search.size()-1);

        while (lower != upper) {
            lower--;
        }

        foxxll::stats_data stats_data = custom_timer.get_data();

        elapsed_time.push_back(stats_data.get_elapsed_time());
        read_count.push_back(stats_data.get_read_count());
        write_count.push_back(stats_data.get_write_count());
    }
    double average_elapsed_time = std::accumulate(elapsed_time.begin(), elapsed_time.end(), 0.0) / elapsed_time.size();
    int average_reads = std::accumulate(read_count.begin(), read_count.end(), 0LL) / read_count.size();
    int average_writes = std::accumulate(write_count.begin(), write_count.end(), 0LL) / write_count.size();

    return std::tuple<double, int, int> { average_elapsed_time, average_reads, average_writes };
}


// ------------------------- BENCHMARKS -------------------------

// Benchmark 1: sequential insertion

void benchmark_1() {
    constexpr unsigned int cachesize = 8 * 4096;
    tree_benchmark b = tree_benchmark("insert", cachesize, "sequential");

    // Have 32kB cache. Insert 32kB to 32 mB
    for (int N=8 * 4096; N <= 32 * 1024 * 1024; N = 2*N) {
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
    tree_benchmark b = tree_benchmark("insert", cachesize, "random");

    // Have 32kB cache. Insert 32kB to 32 mB
    for (int N=8 * 4096; N <= 32 * 1024 * 1024; N = 2*N) {
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
    tree_benchmark b = tree_benchmark("search", cachesize, "sequential");

    // Have 32kB cache. Insert 32kB to 32 mB
    for (int N=8 * 4096; N <= 32 * 1024 * 1024; N = 2*N) {
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
    tree_benchmark b = tree_benchmark("search", cachesize, "random2");

    // Have 32kB cache. Insert 32kB to 32 mB
    for (int N=8 * 4096; N <= 32 * 1024 * 1024; N = 2*N) {
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


// Benchmark 5: range-search

void benchmark_5() {
    constexpr unsigned int cachesize = 8 * 4096;
    tree_benchmark b = tree_benchmark("rangesearch", cachesize, "full");

    // Have 32kB cache. Insert 32kB to 32 mB
    for (int N=8 * 4096; N <= 32 * 1024 * 1024; N = 2*N) {
        // do experiment, get writes and reads for btree and ftree.
        int values_to_insert = N / sizeof(value_type);
        std::vector<value_type> to_insert {};
        to_insert.reserve(values_to_insert);
        for (int i=0; i<values_to_insert; i++)
            to_insert.emplace_back(i,i);

        auto rng = std::default_random_engine { 42 };
        std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

        std::tuple<double, int, int> seconds_reads_writes_ftree = benchmark_ftree_rangesearch<cachesize>(to_insert);
        std::tuple<double, int, int> seconds_reads_writes_btree = benchmark_btree_rangesearch<cachesize>(to_insert);

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


// Benchmark 6: repeated range-search

void benchmark_6() {
    constexpr unsigned int cachesize = 8 * 4096;
    tree_benchmark b = tree_benchmark("rangesearch", cachesize, "fullrepeated");

    // Have 32kB cache. Insert 32kB to 32 mB
    for (int N=8 * 4096; N <= 32 * 1024 * 1024; N = 2*N) {
        // do experiment, get writes and reads for btree and ftree.
        int values_to_insert = N / sizeof(value_type);
        std::vector<value_type> to_insert {};
        to_insert.reserve(values_to_insert);
        for (int i=0; i<values_to_insert; i++)
            to_insert.emplace_back(i,i);

        auto rng = std::default_random_engine { 42 };
        std::shuffle(std::begin(to_insert), std::end(to_insert), rng);

        std::tuple<double, int, int> seconds_reads_writes_ftree = benchmark_ftree_rangesearch_repeated<cachesize>(to_insert);
        std::tuple<double, int, int> seconds_reads_writes_btree = benchmark_btree_rangesearch_repeated<cachesize>(to_insert);

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

int main() {
    benchmark_1();
    benchmark_2();
    benchmark_3();
    benchmark_4();
    benchmark_5();
    benchmark_6();

    return 0;
}
