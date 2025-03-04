#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#include "pot/algorithms/parfor.h"

pot::coroutines::task<int> async_func_1(int x)
{
    printf("Async func 1: %llu\n", std::this_thread::get_id());
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printf("RES 1 IS READY: %d\n", 42 * x);

    co_return 42 * x;
}

pot::coroutines::task<int> async_func_2(int x)
{
    printf("Async func 2: %llu\n", std::this_thread::get_id());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto res = co_await async_func_1(x);
    printf("RES 2 IS READY\n");

    co_return res * 2;
}

TEST_CASE("PARFOR TEST")
{
    constexpr auto vec_size = 10;

    std::vector<double> vec_a(vec_size);
    std::vector<double> vec_b(vec_size);
    std::vector<double> vec_c(vec_size);

    std::ranges::fill(vec_a, 1.0);
    std::ranges::fill(vec_b, 2.0);
    auto clear_c = [&vec_c]
    { std::ranges::fill(vec_c, 0.0); };

    clear_c();

    auto pool = pot::executors::thread_pool_executor_lq("Main");
    auto future = pot::algorithms::parfor(pool, 0, vec_size, [&](int i) -> pot::coroutines::task<void>
                                          {
        printf("THREAD: %ull\n", std::this_thread::get_id());
        vec_c[i] = vec_a[i] + vec_b[i];
        co_return; });
    printf("Future is ready\n");

    REQUIRE(std::all_of(vec_c.begin(), vec_c.end(), [](const auto &v)
                        { return v == 3.0; }));
}
