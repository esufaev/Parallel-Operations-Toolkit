#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "pot/experimental/thread_pool/thread_pool_ol.h"

void printer(const std::string& str)
{
    printf("%s\r\n", str.data());

}

TEST_CASE("pot::experimental::thread_pool::thread_pool_ol")
{
    auto pool = pot::experimental::thread_pool_ol(10);
    for(auto i = 0; i < 10; ++i)
        pool.add_task(printer, std::format("Task {}", i)); // not working with lambda


}
