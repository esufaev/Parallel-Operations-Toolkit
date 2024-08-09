#include <catch2/catch_test_macros.hpp>
#include "pot/tasks/lazy_task.h"
#include "pot/this_thread.h"

#include <thread>
#include <iostream>

TEST_CASE("pot::lazy_task")
{
    auto func = []()
    {
        return 42;
    };

    pot::tasks::lazy_promise<int> promise(func);
    auto task = promise.get_future();

    std::thread t([&promise]()
                  {
                      std::this_thread::sleep_for(std::chrono::milliseconds(500));
                      promise.set_value(42); });

    REQUIRE_FALSE(task.wait_for(std::chrono::milliseconds(100)) == true);
    REQUIRE(task.wait_for(std::chrono::seconds(1)) == true);

    int result = task.get();
    std::cout << result << std::endl;

    REQUIRE(result == 42);

    if (t.joinable())
    {
        t.join();
    }
}