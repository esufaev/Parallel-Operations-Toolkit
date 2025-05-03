#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <stdio.h>
#include <thread>
#include <iostream>

#include "pot/coroutines/task.h"
#include "pot/coroutines/async_condition_variable.h"

pot::coroutines::task<void> waiter(pot::coroutines::async_condition_variable &cv, int id)
{
    std::cout << "Waiter " << id << " is waiting..." << std::endl;
    co_await cv;
    std::cout << "Waiter " << id << " is resumed." << std::endl;
}

pot::coroutines::task<void> setter(pot::coroutines::async_condition_variable &cv)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Setter is setting the condition variable." << std::endl;
    cv.set();
    co_return;
}

TEST_CASE("async_condition_variable", "[coroutines]")
{
    pot::coroutines::async_condition_variable cv;

    std::vector<pot::coroutines::task<void>> tasks;
    for (int i = 0; i < 5; ++i)
    {
        tasks.push_back(waiter(cv, i));
    }

    pot::coroutines::task<void> set_task = setter(cv);

    for (auto &t : tasks)
    {
        t.get(); 
    }
    set_task.get();
}


