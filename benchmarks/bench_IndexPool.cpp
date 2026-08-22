#include <benchmark/benchmark.h>

#include "storage/IndexPool.h"

using namespace winindex;

// Measures index-build throughput: the hot path exercised once per drive scan.
static void BM_IndexPool_AddEntry(benchmark::State& state) {
    const int64_t count = state.range(0);
    for (auto _ : state) {
        state.PauseTiming();
        IndexPool pool;
        pool.Reserve(static_cast<size_t>(count));
        state.ResumeTiming();

        for (int64_t i = 0; i < count; ++i) {
            FileEntry e;
            e.name = L"file" + std::to_wstring(i) + L".txt";
            e.nameLower = e.name;
            e.path = L"C:\\synthetic\\dir" + std::to_wstring(i % 200) + L"\\" + e.name;
            e.size = 1024;
            e.lastModified = 0;
            e.attributes = 0;
            pool.AddEntry(e);
        }
    }
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_IndexPool_AddEntry)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Unit(benchmark::kMillisecond);
