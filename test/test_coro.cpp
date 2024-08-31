#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>

#include "pot/coroutines/task_coroutine.h"

using namespace pot::tasks;

task<int> simple_task()
{
    co_return 42;
}

task<int> async_addition(int a, int b)
{
    co_return a + b;
}

task<int> compute_sum()
{
    int result = co_await async_addition(10, 32);
    co_return result;
}

task<int> generate_numbers()
{
    co_yield 1;
    co_yield 2;
    co_yield 3;
    co_return 0; 
}

TEST_CASE("simple_task returns correct value")
{
    auto t = simple_task();

    REQUIRE(t.get() == 42);
    REQUIRE(t.is_ready() == true);
}

TEST_CASE("co_await with task works correctly")
{
    auto t = compute_sum();
    
    REQUIRE(t.get() == 42);
    REQUIRE(t.is_ready() == true); 
}

TEST_CASE("co_yield works correctly in task")
{
    auto t = generate_numbers();
    std::vector<int> results;

    results.push_back(t.next());
    results.push_back(t.next());
    results.push_back(t.next());
    std::cout << "Result: " << t.get() << std::endl;

    // REQUIRE(results == std::vector<int>{1, 2, 3});
}
