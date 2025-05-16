#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>

#include "pot/executors/thread_pool_executor_lfgq.h"
#include "pot/executors/thread_pool_executor.h"
#include "pot/utils/time_it.h"

template <typename Pool>
long long test_pool(Pool &pool, size_t total_tasks)
{
    std::atomic<size_t> completed(0);

    auto duration = pot::utils::time_it<std::chrono::milliseconds>(5, [] {}, [&]()
    {
        for (size_t i = 0; i < total_tasks; ++i)
        {
            pool.run([&completed]
            {
                double sum = 0.0;
                for (int j = 0; j < 1000; ++j) { sum += std::sin(static_cast<double>(j)); }
                completed.fetch_add(1, std::memory_order_relaxed); 
            });
        }

        while (completed.load(std::memory_order_relaxed) < total_tasks)
            std::this_thread::yield(); 
    }).count();

    return duration;
}

TEST_CASE("PARFOR TEST")
{
    constexpr size_t task_count = 500'000;
    std::vector<size_t> thread_counts = {1, 2, 4, 8, 12, 16, 24, 32};

    printf("%10s | %15s | %15s\n", "Threads", "Time (LFGQ)", "Time (GQ)");
    printf("%s\n", "-----------------------------------------------");

    for (size_t threads : thread_counts)
    {
        pot::executors::thread_pool_executor_lfgq pool_lfgq("lfgq", threads);
        pot::executors::thread_pool_executor_gq pool_gq("gq", threads);

        auto time_lfgq = test_pool(pool_lfgq, task_count);
        auto time_gq = test_pool(pool_gq, task_count);

        printf("%10zu | %15lld | %15lld\n", threads, time_lfgq, time_gq);
    }
}
