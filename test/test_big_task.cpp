#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <thread>
#include <chrono>
#include <iostream>

#include "pot/tasks/big_task.h"  
#include "pot/this_thread.h"

using namespace pot::tasks;

TEST_CASE("big_task progress tracking with interruption")
{
    pot::tasks::big_task_promise<double> promise; 
    auto task = promise.get_task();

    std::thread worker([&promise]()
    {
        for (int i = 0; i <= 10; ++i)
        {
            pot::this_thread::sleep_for(std::chrono::seconds(1)); 
            promise.set_progress(static_cast<double>(i)); 
        }
        promise.set_value(1.0); 
    });

    // Check progress and interrupt the task at 6 seconds
    for (int i = 0; i <= 10; ++i)
    {
        pot::this_thread::sleep_for(std::chrono::seconds(1)); 
        std::cout << "Progress: " << task.get_progress() << std::endl;

        if (i == 6) 
        {
            task.interrupt();
            std::cout << "Interrupted!" << std::endl;
        }
    }

    REQUIRE_THROWS(task.get());
    REQUIRE_THROWS(task.wait());
    REQUIRE_THROWS(task.wait_for(std::chrono::milliseconds(100)));
    REQUIRE_THROWS(task.wait_until(std::chrono::steady_clock::now() + std::chrono::seconds(1)));

    REQUIRE(task.is_interrupted() == true);
    
    if (worker.joinable())
    {
        worker.join(); 
    }
}