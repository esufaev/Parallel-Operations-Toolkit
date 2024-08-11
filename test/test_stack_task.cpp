#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>
#include <exception>
#include <iostream>

#include "pot/tasks/stack_task.h"

TEST_CASE("Set and Get Value")
{
    pot::tasks::stack_task<int> task;

    std::thread t([&task]()
                  {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        task.set_value(42); });

    REQUIRE(task.wait_for(std::chrono::milliseconds(1)) == false);
    task.wait();

    int result = task.get();
    std::cout << result << std::endl;
    REQUIRE(result == 42);

    if (t.joinable())
    {
        t.join();
    }
}

TEST_CASE("Handle Exception")
{
    pot::tasks::stack_task<int> task;

    std::thread t([&task]()
                  {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        task.set_exception(std::make_exception_ptr(std::runtime_error("Test exception"))); });

    REQUIRE(task.wait_for(std::chrono::milliseconds(1)) == false);
    task.wait();

    REQUIRE_THROWS(task.get());
    if (t.joinable())
    {
        t.join();
    }
}

TEST_CASE("Set Value Twice Throws Exception")
{
    pot::tasks::stack_task<int> task;
    task.set_value(42);
    REQUIRE_THROWS(task.set_value(100));
}

TEST_CASE("Set Exception Twice Throws Exception")
{
    pot::tasks::stack_task<int> task;
    task.set_exception(std::make_exception_ptr(std::runtime_error("First exception")));
    REQUIRE_THROWS(task.set_exception(std::make_exception_ptr(std::runtime_error("Second exception"))));
}

TEST_CASE("Get Value Before Setting Throws Exception")
{
    pot::tasks::stack_task<int> task;
    REQUIRE_THROWS(task.get());
}