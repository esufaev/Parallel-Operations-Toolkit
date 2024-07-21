#include <catch2/catch_test_macros.hpp>
#include "pot/future.h"
#include "pot/package_task.h"
#include "pot/promise.h"
#include <iostream>

TEST_CASE("Promise set value test", "[promise]")
{
    pot::promise<int> p;
    auto f = p.get_future();
    p.set_value(42);

    REQUIRE(f.get() == 42);
}