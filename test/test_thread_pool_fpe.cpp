#include <barrier>
#include <latch>

#include <catch2/catch_all.hpp>

#include "pot/experimental/thread_pool/thread_pool_fpe.h"

#include <ranges>

TEST_CASE("pot::experimental::thread_pool::thread_pool_gq_fpe")
{
    constexpr auto thread_count = 10;
    constexpr auto task_count = 10;


    std::vector<std::chrono::steady_clock::time_point> complete_time(task_count);
    const auto clear_time = [&complete_time]() {std::ranges::fill(complete_time, std::chrono::steady_clock::time_point::min()); };

    {
        std::latch latch(task_count);
        clear_time();
        auto pool = pot::experimental::thread_pool::thread_pool_gq_fpe(1);
        for (auto i = 0; i < task_count; ++i)
            pool.run_detached([=, &complete_time, &latch]()
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(task_count - i));
                    complete_time[i] = std::chrono::steady_clock::now();
                    latch.count_down();
                });

        latch.wait();

        REQUIRE(std::ranges::is_sorted(complete_time));
    }


    {
        std::latch latch(task_count);
        clear_time();
        auto pool = pot::experimental::thread_pool::thread_pool_gq_fpe(thread_count);
        for (auto i = 0; i < task_count; ++i)
            pool.run_detached([=, &complete_time, &latch]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(task_count - i));
                complete_time[i] = std::chrono::steady_clock::now();
                latch.count_down();
            });

        latch.wait();

        REQUIRE(!std::ranges::is_sorted(complete_time));
    }

    {
        std::latch latch(task_count);
        clear_time();
        auto pool = pot::experimental::thread_pool::thread_pool_gq_fpe(thread_count);
        for (auto i = 0; i < task_count; ++i)
            pool.run_detached([=, &complete_time, &latch]()
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(task_count - i));
                    complete_time[i] = std::chrono::steady_clock::now();
                    latch.count_down();
                });

        latch.wait();

        REQUIRE(!std::ranges::is_sorted(complete_time));
    }

    {
        std::atomic_int finished = 0;
        std::vector<std::future<void>> futures;
        futures.reserve(task_count);
        auto pool = pot::experimental::thread_pool::thread_pool_gq_fpe(thread_count);
        for (auto i = 0; i < task_count; ++i)
            futures.emplace_back(pool.run([&]() { ++finished; }));

        for (auto& future : futures)
            future.get();

        REQUIRE(finished == task_count);
    }

}

TEST_CASE("pot::experimental::thread_pool::thread_pool_lq_fpe")
{
    constexpr auto thread_count = 10;
    constexpr auto task_count = 10;


    std::vector<std::chrono::steady_clock::time_point> complete_time(task_count);
    const auto clear_time = [&complete_time]() {std::ranges::fill(complete_time, std::chrono::steady_clock::time_point::min()); };

    {
        std::latch latch(task_count);
        clear_time();
        auto pool = pot::experimental::thread_pool::thread_pool_lq_fpe(1);
        for (auto i = 0; i < task_count; ++i)
            pool.run_detached([=, &complete_time, &latch]()
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(task_count - i));
                    complete_time[i] = std::chrono::steady_clock::now();
                    latch.count_down();
                });

        latch.wait();

        REQUIRE(std::ranges::is_sorted(complete_time));
    }


    {
        std::latch latch(task_count);
        clear_time();
        auto pool = pot::experimental::thread_pool::thread_pool_lq_fpe(thread_count);
        for (auto i = 0; i < task_count; ++i)
            pool.run_detached([=, &complete_time, &latch]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(task_count - i));
                complete_time[i] = std::chrono::steady_clock::now();
                latch.count_down();
            });

        latch.wait();

        REQUIRE(!std::ranges::is_sorted(complete_time));
    }

    {
        std::latch latch(task_count);
        clear_time();
        auto pool = pot::experimental::thread_pool::thread_pool_lq_fpe(thread_count);
        for (auto i = 0; i < task_count; ++i)
            pool.run_detached([=, &complete_time, &latch]()
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(task_count - i));
                    complete_time[i] = std::chrono::steady_clock::now();
                    latch.count_down();
                });

        latch.wait();

        REQUIRE(!std::ranges::is_sorted(complete_time));
    }

    {
        std::atomic_int finished = 0;
        std::vector<std::future<void>> futures;
        futures.reserve(task_count);
        auto pool = pot::experimental::thread_pool::thread_pool_lq_fpe(thread_count);
        for (auto i = 0; i < task_count; ++i)
            futures.emplace_back(pool.run([&]() { ++finished; }));

        for (auto& future : futures)
            future.get();

        REQUIRE(finished == task_count);
    }

}

