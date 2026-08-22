#include <benchmark/benchmark.h>

#include "search/SearchEngine.h"
#include "storage/IndexPool.h"
#include <atomic>

using namespace winindex;

namespace {

IndexPool BuildSyntheticPool(int64_t count) {
    IndexPool pool;
    pool.Reserve(static_cast<size_t>(count));
    for (int64_t i = 0; i < count; ++i) {
        FileEntry e;
        e.name = L"document" + std::to_wstring(i) + L".pdf";
        e.nameLower = e.name;
        e.path = L"C:\\synthetic\\dir" + std::to_wstring(i % 200) + L"\\" + e.name;
        e.size = 2048;
        e.lastModified = 0;
        e.attributes = 0;
        pool.AddEntry(e);
    }
    return pool;
}

}  // namespace

// Query and dataset sizes are representative of real usage; see docs/architecture.md
// for the search dispatch this exercises (SIMD substring, RE2 regex).
static void BM_SearchEngine_Substring(benchmark::State& state) {
    const int64_t count = state.range(0);
    IndexPool pool = BuildSyntheticPool(count);
    SearchEngine engine;
    SearchOptions options{};
    std::atomic<bool> cancelToken{false};

    for (auto _ : state) {
        auto results = engine.Search(L"document999", pool.meta.data(), pool.Size(),
                                      pool.nameLowerPool.data(), pool.pathPool.data(), options,
                                      10000, cancelToken);
        benchmark::DoNotOptimize(results);
    }
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_SearchEngine_Substring)->Arg(100000)->Arg(1000000)->Unit(benchmark::kMillisecond);

static void BM_SearchEngine_Regex(benchmark::State& state) {
    const int64_t count = state.range(0);
    IndexPool pool = BuildSyntheticPool(count);
    SearchEngine engine;
    SearchOptions options{};
    options.useRegex = true;
    std::atomic<bool> cancelToken{false};

    for (auto _ : state) {
        auto results = engine.Search(L"document9[0-9]{2}\\.pdf", pool.meta.data(), pool.Size(),
                                      pool.nameLowerPool.data(), pool.pathPool.data(), options,
                                      10000, cancelToken);
        benchmark::DoNotOptimize(results);
    }
    state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_SearchEngine_Regex)->Arg(100000)->Arg(1000000)->Unit(benchmark::kMillisecond);
