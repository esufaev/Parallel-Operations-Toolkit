#include "pot/pot.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>

#include <atomic>
#include <thread>

using pot::coroutines::lazy_task;
using pot::coroutines::resume_on;
using pot::coroutines::task;

TEST_CASE("RESUME_ON TEST")
{
    pot::executors::thread_pool_executor_lflqt ex("Global Executor");

    SECTION("resume_on switches coroutine to executor thread")
    {
        std::thread::id before_id = std::this_thread::get_id();
        std::thread::id after_id{};

        auto t = [&]() -> task<void>
        {
            co_await resume_on(ex);
            after_id = std::this_thread::get_id();
            co_return;
        }();

        t.get();

        REQUIRE(after_id != before_id);
    };

    SECTION("resume_on works with task<int>")
    {
        auto compute = [&]() -> task<int>
        {
            co_await resume_on(ex);
            co_return 123;
        };

        auto t = compute();
        REQUIRE(t.get() == 123);
    };

    SECTION("resume_on works with task<void>")
    {
        std::atomic<int> flag{0};

        auto do_work = [&]() -> task<void>
        {
            co_await resume_on(ex);
            flag.store(42, std::memory_order_relaxed);
            co_return;
        };

        auto t = do_work();
        t.get();

        REQUIRE(flag.load() == 42);
    };

    SECTION("resume_on works with lazy_task<int>")
    {
        auto lazy_compute = [&]() -> lazy_task<int>
        {
            co_await resume_on(ex);
            co_return 77;
        };

        auto lt = lazy_compute();
        REQUIRE(lt.get() == 77);
    };

    SECTION("resume_on multiple awaits")
    {
        std::atomic<int> seq{0};

        auto t = [&]() -> task<int>
        {
            seq.fetch_add(1, std::memory_order_relaxed);
            co_await resume_on(ex);
            seq.fetch_add(1, std::memory_order_relaxed);
            co_await resume_on(ex);
            seq.fetch_add(1, std::memory_order_relaxed);
            co_return seq.load();
        }();

        REQUIRE(t.get() == 3);
    };
}