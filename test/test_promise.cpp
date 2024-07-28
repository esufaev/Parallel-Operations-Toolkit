#include <catch2/catch_test_macros.hpp>

#include "pot/tasks/task.h"
#include "pot/tasks/promise.h"
#include "pot/allocators/shared_allocator.h"

#include <iostream>

TEST_CASE("Promise set value test", "[promise]")
{
    pot::tasks::promise<int, pot::allocators::shared_allocator_for_state<int>> p;
    auto f = p.get_future();
    p.set_value(42);

    int result = f.get();
    std::cout << result << std::endl;

    REQUIRE(result == 42);
}