#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "pot/pot.h"
#include <iostream>

TEST_CASE("POT::THIS_THREAD TEST")
{
    SECTION("test local_id, global_id, name")
    {
        pot::executors::thread_pool_executor_lflqt executor1("Main");
        pot::executors::thread_pool_executor_lflqt executor2("Main");
        pot::algorithms::parfor(executor1, 0, 10,
                                [&](int i)
                                {
                                    std::cout << "Global id: " << pot::this_thread::global_id()
                                              << ", Local id: " << pot::this_thread::local_id()
                                              << ", Name: " << pot::this_thread::name()
                                              << std::endl;
                                })
            .get();

        pot::algorithms::parfor(executor2, 1, 15,
                                [&](int i)
                                {
                                    if (pot::this_thread::local_id() == 4)
                                        pot::this_thread::set_params(SCHED_FIFO, 99);
                                    std::cout << "Global id: " << pot::this_thread::global_id()
                                              << ", Local id: " << pot::this_thread::local_id()
                                              << ", Name: " << pot::this_thread::name()
                                              << std::endl;
                                })
            .get();

        executor1.shutdown();
        executor2.shutdown();
    }
    SECTION("test get and set priority and policy")
    {
        pot::executors::thread_pool_executor_lflqt executor("Main");

        auto task = executor.run(
            [&]()
            {
                auto [current_policy, current_priority] = pot::this_thread::get_params();

                pot::this_thread::set_params(SCHED_RR, 99);

                auto [then_policy, then_priority] = pot::this_thread::get_params();

                REQUIRE(current_policy != then_policy);
                REQUIRE(current_priority != then_priority);
                REQUIRE(then_policy == 2);
                REQUIRE(then_priority == 99);
            });

        task.get();

        executor.shutdown();
    }
}
