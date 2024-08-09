#include <catch2/catch_test_macros.hpp>

#include "pot/tasks/task.h"
#include "pot/package_task.h"
#include "pot/allocators/shared_allocator.h"

#include <iostream>

TEST_CASE("pot::future")
{
    pot::tasks::promise<int, pot::allocators::shared_allocator_for_state<int>> promise;
    auto future = promise.get_future();

    std::thread t([&promise]()
                  {
                      std::this_thread::sleep_for(std::chrono::milliseconds(500));
                      promise.set_value(42);
                  });

    REQUIRE(future.wait_for(std::chrono::milliseconds(1)) == false);
    REQUIRE(future.wait_for(std::chrono::seconds(1)) == true);

    int result = future.get();
    std::cout << result << std::endl;

    REQUIRE(result == 42);

    if (t.joinable())
    {
        t.join();
    }
}