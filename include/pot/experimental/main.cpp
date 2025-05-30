#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#include "pot/coroutines/task.h"
#include "pot/algorithms/parfor.h"
#include "pot/executors/thread_pool_executor_lfgq.h"

pot::executors::thread_pool_executor_lfgq pool("Name");

pot::coroutines::task<void> coro()
{
    std::cout << "Hello from coro!1" << std::endl;
    auto inner = []() -> pot::coroutines::task<void>
    {
        std::cout << "Hello from coro!2" << std::endl;
        co_return;
    };

    co_await inner();
    std::cout << "Inner result: " << std::endl;
}

int main()
{
    auto future = pot::algorithms::parfor(pool, 0, 10, [](int i)
    {
        std::cout << "Hello from parfor! " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    });
    
    future.get();
    std::cout << "DONE" << std::endl;

    return 0;
}