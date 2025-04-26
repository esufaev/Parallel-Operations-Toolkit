#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <stdio.h>
#include <thread>

#include "pot/coroutines/task.h"
#include "pot/coroutines/async_condition_variable.h"

pot::coroutines::task<int> coro1(pot::coroutines::async_condition_variable& cv)
{
    printf("1\n");
    co_await cv;
    printf("2\n");
    co_await cv;
    printf("3\n");
    co_return 12;
}

TEST_CASE("async_condition_variable", "[coroutines]")
{
    pot::coroutines::async_condition_variable cv;
    auto res = coro1(cv);

    cv.set();
    cv.set();
    REQUIRE(res.get() == 12);
}


