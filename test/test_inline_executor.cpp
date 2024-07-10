#include <catch2/catch_test_macros.hpp>

#include "pot/executors/inline_executor.h"

TEST_CASE("pot::executors::inline_executor")
{
    int64_t value = 0;

    auto executor = pot::executors::inline_executor("inline_executor");

    executor.run_detached([&] { value = 1; });

    REQUIRE(value == 1);

    executor.run_detached([&](const int64_t inc) { value += inc; }, 10);

    REQUIRE(value == 11);

    auto future_value_div2 = executor.run([&]() { return value >> 1; });

    REQUIRE(future_value_div2.get() == 5);

    auto future_value_divn = executor.run([&](const int64_t n) { return value / n; }, 2);

    REQUIRE(future_value_divn.get() == 5);
}