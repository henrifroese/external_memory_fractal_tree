/*
 * benchmark_example.cpp
 *
 * Copyright (C) 2020 Hung Tran <hung@ae.cs.uni-frankfurt.de>
 */

#include <benchmark/benchmark.h>

static void BM_StringCreation(benchmark::State& state) {
    for (auto _ : state)
        std::string empty_string;
}
// Register the function as a benchmark
BENCHMARK(BM_StringCreation);

// Define another benchmark
static void BM_StringCopy(benchmark::State& state) {
    std::string x = "hello";
    for (auto _ : state)
        std::string copy(x);
}
BENCHMARK(BM_StringCopy);


static void VectorPushBack(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        for (size_t i = 0; i < state.range(0); ++i) {
            v.push_back(rand());
        }
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(VectorPushBack)->Range(2, 2<<10);

BENCHMARK_MAIN();