#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>

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
        promise.set_value(1.0); });

    for (int i = 0; i <= 10; ++i)
    {
        pot::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Progress: " << task.get_progress() << std::endl;

        if (i == 6)
        {
            task.pause();
            std::cout << "Paused!" << std::endl;
            break;
        }
    }

    REQUIRE(task.get_progress() == 6);

    task.resume();

    worker.join();

    REQUIRE(task.is_interrupted() == false);
    REQUIRE(task.get() == 1.0);

    REQUIRE(task.get_progress() >= 6);
}

TEST_CASE("big_task pause and resume functionality")
{
    pot::tasks::big_task_promise<double> promise;
    auto task = promise.get_task();

    std::thread worker([&promise]()
                       {
        for (int i = 0; i <= 5; ++i)
        {
            pot::this_thread::sleep_for(std::chrono::seconds(1)); 
            promise.set_progress(static_cast<double>(i)); 
        }
        promise.set_value(2.0); });

    task.pause();

    for (int i = 0; i <= 5; ++i)
    {
        pot::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Progress (paused): " << task.get_progress() << std::endl;

        if (i == 3)
        {
            task.resume();
            std::cout << "Resumed!" << std::endl;
            break;
        }
    }

    pot::this_thread::sleep_for(std::chrono::seconds(3));

    REQUIRE(task.get_progress() > 0);
    REQUIRE(task.get() == 2.0);

    if (worker.joinable())
    {
        worker.join();
    }
}