#include <cinttypes>

#include <catch2/catch_all.hpp>

#include "pot/when_all.h"
#include "pot/executors/thread_pool_executor.h"
#include "pot/utils/time_it.h"


template<typename ChronoType>
std::pair<ChronoType, ChronoType> bench_thread_pool_lg(const int64_t test_count, const int64_t thread_count, const int64_t vec_size)
{
    pot::executors::thread_pool_executor_lq executor_local ("thread_pool local", thread_count);
    pot::executors::thread_pool_executor_gq executor_global("thread_pool global", thread_count);

    std::vector<double> vec_a(vec_size);
    std::vector<double> vec_b(vec_size);
    std::vector<double> vec_c(vec_size);

    std::ranges::fill(vec_a, 1.0);
    std::ranges::fill(vec_b, 2.0);
    auto clear_c = [&vec_c] { std::ranges::fill(vec_c, 0.0); };

    clear_c();

    const int64_t chunk_size = std::max(static_cast<int64_t>(1), (vec_size + 1) / thread_count);
    const int64_t numChunks = (vec_size + chunk_size - 1) / chunk_size;

    std::vector<std::future<void>> results;
    results.resize(numChunks);

    auto bench_func = [&](pot::executor* executor, const auto& func)
        {
            for (int64_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex) {
                const int64_t chunkStart = chunkIndex * chunk_size;
                const int64_t chunkEnd = std::min(chunkStart + chunk_size, vec_size);

                results[chunkIndex] = executor->run([=, &func] {
                    for (int64_t i = chunkStart; i < chunkEnd; ++i) {
                        func(i);
                    }
                    });
            }
            pot::when_all(results);
        };

    const auto time_local = pot::utils::time_it<ChronoType>(test_count, clear_c, bench_func, &executor_local, [&](const int64_t i) { vec_c[i] = vec_a[i] + vec_b[i]; });
    const auto time_global = pot::utils::time_it<ChronoType>(test_count, clear_c, bench_func, &executor_global, [&](const int64_t i) { vec_c[i] = vec_a[i] + vec_b[i]; });

    return {time_local, time_global};
}


TEST_CASE("pot::executors::bench_thread_pool_executor")
{
    for (int64_t&& vec_size : { 1e2, 1e3, 1e4, 1e6 })
    {
        constexpr auto column_width = 10;

        printf("\n\nvec_size = %" PRId64 "\n\n", vec_size);

        printf("%*s\t", column_width, "threads");
        printf("%*s\t", column_width, "local"  );
        printf("%*s\n", column_width, "global" );

        for(int64_t&& thread_count : {1, 2, 4, 8, 12, 16, 24, 32, 64, 128, 256, 512})
        {
            constexpr int64_t test_count = 1000;
            const auto [time_local, time_global] = bench_thread_pool_lg<std::chrono::nanoseconds>(test_count, thread_count, vec_size);

            printf("%*" PRId64 "\t", column_width, thread_count);
            printf("%*" PRId64 "\t", column_width, time_local.count());
            printf("%*" PRId64 "\n", column_width, time_global.count());
        } // for thread_count
    } // for vec_size

    // BENCHMARK_ADVANCED("thread_pool_executor local add")(Catch::Benchmark::Chronometer meter)
    // {
    //     meter.measure([&]{bench_func(&executor_local, [&](const int64_t i) { vec_c[i] = vec_a[i] + vec_b[i]; }); });
    //     clear_c();
    // };
    //
    // BENCHMARK_ADVANCED("thread_pool_executor global add")(Catch::Benchmark::Chronometer meter)
    // {
    //     meter.measure([&]{bench_func(&executor_global, [&](const int64_t i) { vec_c[i] = vec_a[i] + vec_b[i]; }); });
    //     clear_c();
    // };

}