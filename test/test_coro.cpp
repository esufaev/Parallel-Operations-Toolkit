#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#include "pot/algorithms/parfor.h"
#include "pot/executors/thread_pool_executor_lfgq.h"
#include "pot/coroutines/task.h"

TEST_CASE("CO_AWAIT LAZY TEST")
{
    auto inner_task_val = []() -> pot::coroutines::lazy_task<int>
    {
        std::cout << "inner_task started" << std::endl;
        co_return 42;
    };

    auto outer_task_val = [&]() -> pot::coroutines::lazy_task<int>
    {
        std::cout << "outer_task started" << std::endl;
        int result = co_await inner_task_val();
        std::cout << "outer_task finished" << std::endl;
        co_return result;
    };

    REQUIRE(outer_task_val().get() == 42);

    auto inner_task_void = []() -> pot::coroutines::lazy_task<void>
    {
        std::cout << "inner_task" << std::endl;
        co_return;
    };

    auto outer_task_void = [&]() -> pot::coroutines::lazy_task<void>
    {
        std::cout << "outer_task started" << std::endl;
        co_await inner_task_void();
        std::cout << "outer_task finished" << std::endl;
        co_return;
    };

    outer_task_void().get();
}

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

TEST_CASE("PARFOR VOID TEST")
{
    pot::executors::thread_pool_executor_lfgq pool("Main");

    auto task = pot::algorithms::parfor(pool, 0, 3, [](int i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "i: " << i << std::endl;
    });

    task.get();
    std::cout << "DONE" << std::endl;
}

TEST_CASE("PARFOR VOID CORO TEST")
{
    const int64_t from = 0;
    const int64_t to = 100;

    std::atomic<int64_t> sum = 0;

    pot::executors::thread_pool_executor_lfgq pool("Main");

    auto task = pot::algorithms::parfor(pool, from, to, [](int i) -> pot::coroutines::task<void>
    { 
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        printf("I: %d\n\r", i);
        co_return;
    });

    task.get();
    std::cout << "DONE" << std::endl;
}

TEST_CASE("PARFOR CAPTURED LAMBDA TEST")
{
    std::atomic<int> counter = 0;
    pot::executors::thread_pool_executor_lfgq pool("Main");
    auto task = pot::algorithms::parfor(pool, 0, 1000, [&counter](int i)
    {
        printf("I: %d\n\r", i);
        counter.fetch_add(1); 
    });
    task.get();
    std::cout << "DONE" << std::endl;
    std::cout << counter.load() << std::endl;
    REQUIRE(counter.load() == 1000);
}

TEST_CASE("PARFOR CAPTURED LAMBDA CORO TEST")
{
    std::atomic<int> counter = 0;   

    pot::executors::thread_pool_executor_lfgq pool("Main");
    auto task = pot::algorithms::parfor(pool, 0, 1000, [&counter](int i) -> pot::coroutines::task<void>
    {
        std::cout << "i: " << i << std::endl;
        counter.fetch_add(1);
        co_return;
    });

    task.get();
    std::cout << "DONE" << std::endl;

    REQUIRE(counter.load() == 1000);
}