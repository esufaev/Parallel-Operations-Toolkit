#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "pot/experimental/thread_pool/thread_pool_GQ_fpe.h"
#include "pot/experimental/thread_pool/thread_pool_ol.h"

void printer(const std::string& str)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    printf("%s\r\n", str.data());
}

TEST_CASE("pot::experimental::thread_pool::thread_pool_ol")
{
    constexpr auto thread_count = 10;
    constexpr auto task_count = 10;
    auto pool = pot::experimental::thread_pool_ol(thread_count);
    for(auto i = 0; i < task_count; ++i)
        pool.add_task(printer, std::format("Task {}", i)); // not working with lambda


}

TEST_CASE("pot::experimental::thread_pool::thread_pool_gq_fpe")
{
    constexpr auto thread_count = 10;
    constexpr auto task_count = 10;


    printf("run detached\r\n");
    auto pool = pot::experimental::thread_pool::thread_pool_gq_fpe(thread_count);
    for(auto i = 0; i < task_count; ++i)
        pool.run_detached(printer, std::format("Task {}", i));


    printf("run no future\r\n");
    for(auto i = 0; i < task_count; ++i)
        pool.run(printer, std::format("Task {}", i));


    printf("run with future\r\n");
    std::vector<std::future<void>> futures;
    futures.reserve(task_count);
    for(auto i = 0; i < task_count; ++i)
        futures.emplace_back(pool.run(printer, std::format("Task {}", i)));

    printf("future waiting\r\n");
    for (auto& future : futures)
        future.get();

}
