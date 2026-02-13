#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>
#include <thread>

#include "pot/coroutines/resume_on.h"
#include "pot/coroutines/task.h"
#include "pot/executors/thread_pool_executor.h"

TEST_CASE("Resume on")
{
    pot::executors::thread_pool_executor_gq exec("LogicPool", 2);
    std::thread::id main_id = std::this_thread::get_id();

    SECTION("Start on Main -> Switch to Executor (Named Lambda Approach)")
    {

        std::atomic<bool> switched{false};

        auto manual_task = [&](pot::executor &ex) -> pot::coroutines::task<void>
        {
            if (std::this_thread::get_id() != main_id)
                co_return;

            co_await pot::coroutines::resume_on(ex);

            if (std::this_thread::get_id() != main_id)
            {
                switched.store(true, std::memory_order_release);
            }
        };

        auto t = manual_task(exec);
        t.get();

        REQUIRE(switched.load());
    }

    SECTION("Executor A -> Executor B (Ping Pong via exec.run)")
    {
        pot::executors::thread_pool_executor_gq exec_b("PoolB", 1);

        auto t = exec.run(
            [&]() -> pot::coroutines::task<void>
            {
                auto id_a = std::this_thread::get_id();
                REQUIRE(id_a != main_id);

                co_await pot::coroutines::resume_on(exec_b);

                auto id_b = std::this_thread::get_id();
                REQUIRE(id_b != main_id);
                REQUIRE(id_b != id_a);

                co_await pot::coroutines::resume_on(exec);

                auto id_final = std::this_thread::get_id();
                REQUIRE(id_final != id_b);
                REQUIRE(id_final != main_id);
            });

        t.get();
    }
}
