#include <catch2/catch_test_macros.hpp>
#include "pot/future.h"
#include "pot/package_task.h"
#include <iostream>

TEST_CASE("pot::future")
{
    pot::packaged_task<int> task([]()
                                 { std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                 return 42; });
    pot::future<int> future = task.get_future();
    std::thread t(std::move(task));
    t.detach();
    REQUIRE(future.wait_for(std::chrono::milliseconds(1)) == false);
    REQUIRE(future.wait_for(std::chrono::seconds(1)) == true);

    int result = future.get();
    std::cout << result << std::endl;

    REQUIRE(result == 42);
}