#include <atomic>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "pot/algorithms/parfor.h"
#include "pot/executors/thread_pool_executor.h"

TEST_CASE("Parfor: Concurrency and Thread Distribution", "[parfor]")
{
    pot::executors::thread_pool_executor_gq pool("parfor_pool", 4);

    SECTION("Executes on multiple threads")
    {
        std::mutex mtx;
        std::set<std::thread::id> thread_ids;
        const int iterations = 100;

        pot::algorithms::parfor(pool, 0, iterations,
                                [&](int)
                                {
                                    std::lock_guard lock(mtx);
                                    thread_ids.insert(std::this_thread::get_id());
                                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                                })
            .get();

        REQUIRE(thread_ids.size() > 1);
        REQUIRE(thread_ids.size() <= 4);
    }
}

TEST_CASE("Parfor: Data Integrity and Captures", "[parfor]")
{
    pot::executors::thread_pool_executor_gq pool("data_pool");

    SECTION("Correctly handles Capture by Reference (Shared State)")
    {
        const int size = 1000;
        std::vector<int> data(size, 0);

        pot::algorithms::parfor(pool, 0, size, [&](int i) { data[i] = i * 2; }).get();

        for (int i = 0; i < size; ++i)
        {
            REQUIRE(data[i] == i * 2);
        }
    }

    SECTION("Atomic Accumulation (Race Check)")
    {
        const int size = 10000;
        std::atomic<int> counter{0};

        pot::algorithms::parfor(pool, 0, size,
                                [&](int) { counter.fetch_add(1, std::memory_order_relaxed); })
            .get();

        REQUIRE(counter.load() == size);
    }

    SECTION("Static Chunk Size check")
    {
        std::atomic<int> counter{0};
        const int size = 50000;

        pot::algorithms::parfor(pool, 0, size,
                                [&](int) { counter.fetch_add(1, std::memory_order_relaxed); })
            .get();

        REQUIRE(counter.load() == size);
    }
}

TEST_CASE("Parfor: Async Body (Coroutines inside loop)", "[parfor][async]")
{
    pot::executors::thread_pool_executor_gq pool("async_pool", 4);

    SECTION("Loop body returns task<void> (Implicit await)")
    {
        std::atomic<int> counter{0};
        const int size = 20;

        pot::algorithms::parfor(pool, 0, size,
                                [&](int) -> pot::coroutines::task<void>
                                {
                                    co_await pool.run([] {});
                                    counter.fetch_add(1, std::memory_order_relaxed);
                                    co_return;
                                })
            .get();

        REQUIRE(counter.load() == size);
    }

    SECTION("Mixed workload")
    {
        std::vector<int> results(50, 0);

        pot::algorithms::parfor(pool, 0, 50, [&](int i) -> pot::coroutines::task<void>
                                { co_await pool.run([&, i] { results[i] = i + 1; }); })
            .get();

        for (int i = 0; i < 50; ++i)
        {
            REQUIRE(results[i] == i + 1);
        }
    }
}

TEST_CASE("Parfor: Usage via co_await vs .get()", "[parfor]")
{
    pot::executors::thread_pool_executor_gq pool("usage_pool", 2);

    SECTION("Called via .get() (Blocking)")
    {
        bool done = false;
        pot::algorithms::parfor(pool, 0, 1, [&](int) { done = true; }).get();
        REQUIRE(done);
    }

    SECTION("Called via co_await (Composition)")
    {
        std::atomic<int> sum{0};
        const int count = 100;

        auto run_wrapper = [&]() -> pot::coroutines::task<void>
        {
            co_await pot::algorithms::parfor(pool, 0, count, [&](int i)
                                             { sum.fetch_add(1, std::memory_order_relaxed); });
            co_return;
        };

        run_wrapper().get();
        REQUIRE(sum.load() == count);
    }

    SECTION("Nested parfor (Recursive parallelism)")
    {
        std::atomic<int> total_ops{0};
        const int outer_iters = 500;
        const int inner_iters = 100;

        pot::algorithms::parfor(pool, 0, outer_iters,
                                [&](int) -> pot::coroutines::task<void>
                                {
                                    co_await pot::algorithms::parfor(
                                        pool, 0, inner_iters, [&](int)
                                        { total_ops.fetch_add(1, std::memory_order_relaxed); });
                                })
            .get();

        REQUIRE(total_ops.load() == outer_iters * inner_iters);
    }
}

TEST_CASE("Parfor: Edge Cases", "[parfor][edge]")
{
    pot::executors::thread_pool_executor_gq pool("edge_pool", 2);

    SECTION("Single Iteration")
    {
        int val = 0;
        pot::algorithms::parfor(pool, 10, 11, [&](int i) { val = i; }).get();
        REQUIRE(val == 10);
    }

    SECTION("Chunk larger than range")
    {
        std::atomic<int> count{0};
        pot::algorithms::parfor<100>(pool, 0, 5, [&](int) { count++; }).get();
        REQUIRE(count == 5);
    }
}
