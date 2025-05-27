#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#include "pot/algorithms/parfor.h"
#include "pot/executors/thread_pool_executor_lfgq.h"

TEST_CASE("PARFOR TEST")
{
    printf("Started\n\r");
    pot::executors::thread_pool_executor_lfgq pool("Main");
    pot::algorithms::parfor(pool, 0, 3, [](int i) -> pot::coroutines::task<void>
    {   
        printf("%d\n\r", i);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        co_return;
    }).get();    
}
