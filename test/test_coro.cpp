#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#include "pot/algorithms/parfor.h"
#include "pot/executors/thread_pool_executor_lfgq.h"
#include "pot/coroutines/task.h"

TEST_CASE("GET LAZY TEST")
{
    auto future = []() -> pot::coroutines::lazy_task<int>
    {
        std::cout << "future started" << std::endl;
        co_return 42;
    }();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "future created" << std::endl;
    REQUIRE(future.get() == 42);
}

TEST_CASE("GET TEST")
{
    auto future = []() -> pot::coroutines::task<int>
    {
        std::cout << "future started" << std::endl;
        co_return 42;
    }();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "future created" << std::endl;
    REQUIRE(future.get() == 42);
}

TEST_CASE("GET VOID TEST")
{
    auto future = []() -> pot::coroutines::task<void>
    {
        std::cout << "future started" << std::endl;
        co_return;
    }();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "future created" << std::endl;
    future.get();
}

TEST_CASE("GET LAZY VOID TEST")
{
    auto future = []() -> pot::coroutines::lazy_task<void>
    {
        std::cout << "future started" << std::endl;
        co_return;
    }();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "future created" << std::endl;
    future.get();
}



TEST_CASE("VOID TEST")
{
    const int64_t from = 0;
    const int64_t to = 1000;

    std::atomic<int64_t> sum = 0;

    pot::executors::thread_pool_executor_lfgq pool("Main", 4);

    auto task = pot::algorithms::parfor(pool, from, to, [](int64_t i)
    { 
        std::cout << "i: " << i << std::endl;
    });

    task.get();
}

// Последовательно?
TEST_CASE("VOID CORO TEST")
{
    const int64_t from = 0;
    const int64_t to = 1000;

    std::atomic<int64_t> sum = 0;

    pot::executors::thread_pool_executor_lfgq pool("Main", 4);

    auto task = pot::algorithms::parfor(pool, from, to, [](int64_t i) -> pot::coroutines::task<void>
    { 
        std::cout << "i: " << i << std::endl; 
        co_return;
    });

    task.get();
}

