#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <vector>
#include <cassert>
#include <coroutine>
#include <thread>
#include <chrono>
#include <mutex>

#include "pot/executors/thread_pool_executor.h"
#include "pot/algorithms/parfor.h"

void example_function(int i)
{
    std::cout << "Processing " << i << " in thread " << std::this_thread::get_id() << std::endl;
}

pot::coroutines::task<int> test_parfor()
{
    std::cout << "Starting test_parfor..." << std::endl;

    pot::executors::thread_pool_executor_gq executor("Test Thread Pool");

    const int from = 0;
    const int to = 10;

    std::vector<int> results(to - from, -1);

    std::mutex mtx;

    auto fill_results = [&results, &mtx](int i)
    {
        std::lock_guard<std::mutex> lock(mtx);
        results[i] = i;
        std::cout << "Processed index " << i << " in thread " << std::this_thread::get_id() << std::endl;
    };

    std::cout << "Calling parfor..." << std::endl;
    co_await pot::algorithms::parfor(executor, from, to, fill_results);
    std::cout << "parfor completed." << std::endl;

    for (int i = from; i < to; ++i)
    {
        REQUIRE(results[i] == i);
        std::cout << "Verified index " << i << " with value " << results[i] << std::endl;
    }

    std::cout << "parfor test passed!" << std::endl;

    co_return 0;
}

TEST_CASE("pot::bench_coroutines_executor")
{
    std::cout << "Starting test case..." << std::endl;
    auto task = test_parfor();
    task.get();
    std::cout << "Test case completed." << std::endl;
}
