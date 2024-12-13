#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <vector>
#include <cassert>
#include <coroutine>
#include <thread>
#include <chrono>
#include <mutex>

#include "pot/executors/thread_pool_executor.h"

pot::coroutines::task<int> async_computation(int value)
{
    std::cout << "Starting coroutine 1..." << std::endl;
    std::cout << "This thread: " << std::this_thread::get_id() << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(2));

    int result = value * 2;
    std::cout << "Coroutine 1 finished with result: " << result << std::endl;
    co_return result;
}

TEST_CASE("pot::bench_nested_coroutines_executor")
{
    std::cout << "Main thread: " << std::this_thread::get_id() << std::endl;

    pot::executors::thread_pool_executor_lq executor("Main");

    auto future = executor.run([]()
                 { return async_computation(5); });
    
    std::cout << "RESULT: " << future.get() << std::endl;

    REQUIRE(future.get() == 10);
}
