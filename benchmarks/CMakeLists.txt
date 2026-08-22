add_executable(winindex_benchmarks
    bench_IndexPool.cpp
    bench_SearchEngine.cpp
)

target_link_libraries(winindex_benchmarks
    PRIVATE
        winindex_core
        benchmark::benchmark
        benchmark::benchmark_main
)
