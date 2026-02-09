#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "pot/executors/thread_pool_executor.h"
#include "pot/threads/thread.h"

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

TEST_CASE("pot::thread tests")
{
    SECTION("Basic execution")
    {
        pot::thread t;
        std::atomic<bool> executed{false};

        t.run([&]() { executed = true; });

        t.join();
        REQUIRE(executed.load());
    }

    SECTION("Argument passing")
    {
        pot::thread t;
        std::atomic<int> result{0};

        t.run([&](int a, int b) { result = a + b; }, 10, 32);

        t.join();
        REQUIRE(result.load() == 42);
    }

    SECTION("Argument capturing (Strings)")
    {
        pot::thread t;
        std::string output;

        std::string s1 = "Hello";
        std::string s2 = "World";

        t.run([&](std::string a, std::string b) { output = a + " " + b; }, s1, s2);

        t.join();
        REQUIRE(output == "Hello World");
    }

    SECTION("FIFO Order & Destructor Drain")
    {
        std::vector<int> results;

        {
            pot::thread t;
            const int count = 50;

            for (int i = 0; i < count; ++i)
            {
                t.run(
                    [&, i]()
                    {
                        if (i % 10 == 0)
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));

                        results.push_back(i);
                    });
            }
        }

        REQUIRE(results.size() == 50);
        for (int i = 0; i < 50; ++i)
        {
            REQUIRE(results[i] == i);
        }
    }

    SECTION("Multi-threaded Producer (Stress Test)")
    {
        pot::thread t;
        std::atomic<int> counter{0};
        const int num_producers = 10;
        const int tasks_per_producer = 1000;

        std::vector<std::thread> producers;
        producers.reserve(num_producers);

        for (int i = 0; i < num_producers; ++i)
        {
            producers.emplace_back(
                [&]()
                {
                    for (int j = 0; j < tasks_per_producer; ++j)
                    {
                        t.run([&]() { counter.fetch_add(1); });
                    }
                });
        }

        for (auto &p : producers)
        {
            if (p.joinable())
                p.join();
        }

        t.join();

        REQUIRE(counter.load() == num_producers * tasks_per_producer);
    }

    SECTION("Recursive run (task scheduling another task)")
    {
        pot::thread t;
        std::atomic<int> counter{0};

        t.run(
            [&t, &counter]()
            {
                counter++;
                t.run([&counter]() { counter++; });
            });

        t.join();
        REQUIRE(counter.load() == 2);
    }
}

// TEST_CASE("pot::thread executor info tests")
// {
//     SECTION("Thread knows its executor name")
//     {
//         const std::string exec_name = "MySuperExecutor";
//         const std::string thread_name = "Worker-X";
//
//         pot::executors::thread_pool_executor_lq my_exec(exec_name);
//
//         pot::thread t(thread_name, 5, &my_exec);
//
//         std::string actual_exec_name;
//         std::string actual_thread_name;
//
//         t.run(
//             [&]()
//             {
//                 actual_exec_name = pot::this_thread::executor_name();
//                 actual_thread_name = pot::this_thread::name();
//             });
//
//         t.join();
//
//         REQUIRE(actual_exec_name == exec_name);
//         REQUIRE(actual_thread_name == thread_name);
//     }
//
//     SECTION("Thread without executor returns default/None")
//     {
//         pot::thread t;
//         std::string exec_name;
//
//         t.run([&]() { exec_name = pot::this_thread::executor_name(); });
//
//         t.join();
//
//         REQUIRE((exec_name == "None" || exec_name.empty()));
//     }
// }
//
// TEST_CASE("pot::thread extended tests")
// {
//     SECTION("Thread Name & Local ID Initialization")
//     {
//         const std::string expected_name = "Worker-01";
//         const int64_t expected_id = 777;
//
//         pot::thread t(expected_name, expected_id);
//
//         std::string actual_name;
//         int64_t actual_id = -1;
//
//         t.run(
//             [&]()
//             {
//                 actual_name = pot::this_thread::name();
//                 actual_id = pot::this_thread::local_id();
//             });
//
//         t.join();
//
//         REQUIRE(actual_name == expected_name);
//         REQUIRE(actual_id == expected_id);
//     }
//
//     SECTION("Dynamic Name Change")
//     {
//         pot::thread t("OldName");
//
//         std::string name_step_1;
//         std::string name_step_2;
//
//         t.run([&]() { name_step_1 = pot::this_thread::name(); });
//
//         t.set_name("NewName");
//
//         t.run([&]() { name_step_2 = pot::this_thread::name(); });
//
//         t.join();
//
//         REQUIRE(name_step_1 == "OldName");
//         REQUIRE(name_step_2 == "NewName");
//     }
//
//     SECTION("Thread Params (Priority/Policy)")
//     {
//         pot::thread t;
//         bool success = false;
//         std::pair<int, int> params;
//
//         t.run(
//             [&]()
//             {
//                 success = pot::this_thread::set_params(SCHED_OTHER, 0);
//                 params = pot::this_thread::get_params();
//             });
//
//         t.join();
//
//         if (success)
//         {
//             REQUIRE(params.first == SCHED_OTHER);
//             REQUIRE(params.second == 0);
//         }
//         else
//         {
//             SUCCEED("set_params failed (likely due to permissions), but executed cleanly");
//         }
//     }
// }
