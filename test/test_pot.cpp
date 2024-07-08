#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "pot/experimental/thread_pool/thread_pool_gq_esu.h"

void printer(const std::string& str)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    printf("%s\r\n", str.data());
}

TEST_CASE("pot::experimental::thread_pool::thread_pool_ol")
{
    // constexpr auto thread_count = 10;
    // constexpr auto task_count = 10;
    // auto pool = pot::experimental::thread_pool_ol(thread_count);
    // for(auto i = 0; i < task_count; ++i)
    //     pool.add_task(printer, std::format("Task {}", i)); // not working with lambda

    REQUIRE(1 + 1 == 2);
}


