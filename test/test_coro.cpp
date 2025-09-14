#include "pot/pot.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>
#include <numeric>
#include <type_traits>

pot::coroutines::task<int> add_one_task(int x) { co_return x + 1; }
pot::coroutines::lazy_task<int> add_one_lazy(int x) { co_return x + 1; }

using pot::coroutines::lazy_task;
using pot::coroutines::task;

TEST_CASE("CORO")
{
    SECTION("task<int> returns value via get() and await")
    {
        auto t = []() -> task<int>
        { co_return 42; }();
        REQUIRE(t.get() == 42);

        auto outer = [&]() -> task<int>
        {
            int v = co_await []() -> task<int>
            { co_return 100; }();
            co_return v + 1;
        }();
        REQUIRE(outer.get() == 101);
    }

    SECTION("task propagates exceptions through get()/await")
    {
        auto failing = []() -> task<int>
        {
            throw std::runtime_error("boom");
            co_return 0;
        }();

        REQUIRE_THROWS_AS(failing.get(), std::runtime_error);

        auto proxy = [&]() -> task<void>
        {
            try
            {
                (void)co_await []() -> task<int>
                {
                    throw std::logic_error("bad");
                    co_return 0;
                }();
                FAIL("exception not thrown");
            }
            catch (const std::logic_error &)
            {
                co_return;
            }
        }();
        REQUIRE_NOTHROW(proxy.get());
    }

    SECTION("lazy_task is lazy until get()/await")
    {
        std::atomic<bool> started{false};
        auto make = [&]() -> lazy_task<int>
        {
            started.store(true, std::memory_order_relaxed);
            co_return 7;
        };
        auto lt = make();

        REQUIRE(started.load(std::memory_order_relaxed) == false);
        REQUIRE(lt.get() == 7);
        REQUIRE(started.load(std::memory_order_relaxed) == true);
    }

    SECTION("lazy_task<void> await + continuation resumes caller")
    {
        std::atomic<int> seq{0};

        auto child = [&]() -> lazy_task<void>
        {
            seq.fetch_add(1, std::memory_order_relaxed);
            co_return;
        };

        auto parent = [&]() -> task<int>
        {
            seq.fetch_add(1, std::memory_order_relaxed);
            co_await child();
            seq.fetch_add(1, std::memory_order_relaxed);
            co_return seq.load(std::memory_order_relaxed);
        }();

        REQUIRE(parent.get() == 3);
    }

    SECTION("parfor processes all indices (void functor)")
    {
        pot::executors::thread_pool_executor_lflqt ex("Main");
        const int N = 1000;
        std::vector<int> data(N, 0);

        auto set_i = [&](int i)
        {
            data[static_cast<std::size_t>(i)] = i;
        };

        auto task_par = pot::algorithms::parfor(ex, 0, N, set_i);
        REQUIRE_NOTHROW(task_par.get());

        for (int i = 0; i < N; ++i)
        {
            REQUIRE(data[static_cast<std::size_t>(i)] == i);
        }
    }

    SECTION("parfor supports functor returning task<void>")
    {
        pot::executors::thread_pool_executor_lflqt ex("Main");
        const int N = 512;
        std::atomic<int> sum{0};

        auto body_task = [&](int i) -> task<void>
        {
            sum.fetch_add(i, std::memory_order_relaxed);
            co_return;
        };

        auto lt = pot::algorithms::parfor(ex, 0, N, body_task);
        lt.get();

        const long long expected = 1LL * N * (N - 1) / 2;
        REQUIRE(sum.load(std::memory_order_relaxed) == expected);
    }

    SECTION("parfor respects chunking math (small ranges)")
    {
        pot::executors::thread_pool_executor_lflqt ex("Main");

        const int from = 5, to = 13;
        std::vector<int> out(to - from, -1);

        auto body = [&](int i)
        {
            out[static_cast<std::size_t>(i - from)] = i * 2;
        };

        auto lt = pot::algorithms::parfor(ex, from, to, body);
        lt.get();

        for (int i = 0; i < (to - from); ++i)
        {
            REQUIRE(out[static_cast<std::size_t>(i)] == (from + i) * 2);
        }
    }

    SECTION("parfor works with lazy functor returning lazy_task<void>")
    {
        pot::executors::thread_pool_executor_lflqt ex("Main");

        const int N = 64;
        std::atomic<int> cnt{0};

        auto lazy_body = [&](int /*i*/) -> lazy_task<void>
        {
            cnt.fetch_add(1, std::memory_order_relaxed);
            co_return;
        };

        auto lt = pot::algorithms::parfor(ex, 0, N, lazy_body);
        lt.get();

        REQUIRE(cnt.load(std::memory_order_relaxed) == N);
    }
}
