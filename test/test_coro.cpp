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
#include "pot/coroutines/lock_guard_coro.h"  

pot::coroutines::task<void> test_nested_parfor(pot::executor &executor)
{
    const int outer_from = 0, outer_to = 3;
    const int inner_from = 0, inner_to = 10;

    pot::coroutines::async_lock lock; 

    std::cout << "Started" << std::endl;

    co_await pot::algorithms::parfor(executor, outer_from, outer_to, [&](int outer_index) -> pot::coroutines::task<void>
    {
        co_await pot::algorithms::parfor(executor, inner_from, inner_to, [&, outer_index](int inner_index) -> pot::coroutines::task<void>
        {
            {
                co_await lock.lock();  
                pot::coroutines::lock_guard guard(lock); 

                std::cout << "o/i: " << outer_index << " " << inner_index << " Started" << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::seconds(3)); 

            {
                co_await lock.lock();
                pot::coroutines::lock_guard guard(lock);

                std::cout << "o/i: " << outer_index << " " << inner_index << " Finished" << std::endl;
            }
            co_return;
        });
        co_return;
    });

    std::cout << "Nested parfor test passed!" << std::endl;

    co_return;
}

pot::coroutines::task<void> func()
{
    pot::executors::thread_pool_executor_gq executor("Main");

    auto test_task = test_nested_parfor(executor);

    co_await test_task;

    executor.shutdown();
    co_return;
}

TEST_CASE("pot::bench_nested_coroutines_executor")
{
    func();
}
