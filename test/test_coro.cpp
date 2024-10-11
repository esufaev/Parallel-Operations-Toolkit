#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <vector>
#include <cassert>
#include <coroutine>
#include <thread>
#include <chrono>

#include "pot/executors/thread_pool_executor.h"
#include "pot/algorithms/parfor.h"

pot::coroutines::task<void> test_nested_parfor(pot::executor &executor)
{
    const int outer_from = 0, outer_to = 10;
    const int inner_from = 0, inner_to = 10;

    std::vector<std::vector<int>> results(outer_to - outer_from, std::vector<int>(inner_to - inner_from, 0));

    co_await pot::algorithms::parfor(executor, outer_from, outer_to, [&](int outer_index) -> pot::coroutines::task<void>
    {
        co_await pot::algorithms::parfor(executor, inner_from, inner_to, [&](int inner_index)
        {
            results[outer_index][inner_index]++;
        });
    });

    for (int i = outer_from; i < outer_to; ++i)
    {
        for (int j = inner_from; j < inner_to; ++j)
        {
            assert(results[i][j] == 1);
        }
    }

    std::cout << "Nested parfor test passed!" << std::endl;
}

TEST_CASE("pot::bench_nested_coroutines_executor")
{
    pot::executors::thread_pool_executor_gq executor("Main", 4);

    auto test_task = test_nested_parfor(executor);
    test_task.wait();

    executor.shutdown();
}
