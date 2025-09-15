#include "pot/pot.h"

#include <catch2/catch_test_macros.hpp>

using namespace pot::sync;
using namespace pot::coroutines;


#include <iostream>

TEST_CASE("ASYNC_LOCK")
{
    SECTION("exclusive access and ordering")
    {
        async_lock lock;
        std::vector<int> order;

        auto coro = [&](int id) -> task<void>
        {
            auto g = co_await lock.lock();
            order.push_back(id);
        };

        auto t1 = coro(1);
        auto t2 = coro(2);
        auto t3 = coro(3);

        t1.get();
        t2.get();
        t3.get();

        REQUIRE(order.size() == 3);
        REQUIRE(order[0] == 1);
        REQUIRE(order[1] == 2);
        REQUIRE(order[2] == 3);
    }

    SECTION("try_lock")
    {
        async_lock lock;
        REQUIRE(lock.try_lock() == true);
        REQUIRE(lock.try_lock() == false);
    }

    SECTION("many coroutines updating shared vector")
    {
        pot::executors::thread_pool_executor_lflqt ex("Main", 12, 1<<12);
        pot::sync::async_lock al;
        int var = 0;
        pot::algorithms::parfor(ex, 0, 1000, [&](int i) -> pot::coroutines::task<void>
        {
            auto guard = co_await al.lock();
            var += i * 12;
            co_return;
        }).get();

        REQUIRE(var == 5994000);
    }
}
