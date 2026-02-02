#include <catch2/catch_all.hpp>
#include <vector>

#include "pot/algorithms/parfor.h"
#include "pot/coroutines/task.h"
#include "pot/executors/thread_pool_executor.h"
#include "pot/sync/async_lock.h"

TEST_CASE("pot::sync::async_lock tests")
{
    SECTION("Basic lock usage (single coroutine)")
    {
        pot::sync::async_lock lock;
        bool locked_section_entered = false;

        auto t = [&]() -> pot::coroutines::task<void>
        {
            {
                auto guard = co_await lock.lock();
                locked_section_entered = true;
            }
            co_return;
        }();

        t.get();
        REQUIRE(locked_section_entered);
    }

    SECTION("Scoped lock guard move semantics")
    {
        pot::sync::async_lock lock;
        int counter = 0;

        auto t = [&]() -> pot::coroutines::task<void>
        {
            auto guard1 = co_await lock.lock();
            counter++;

            {
                pot::sync::async_lock::scoped_lock_guard guard2(std::move(guard1));
            }

            auto guard3 = co_await lock.lock();
            counter++;

            pot::sync::async_lock::scoped_lock_guard guard4(lock);
        };

        auto move_test_task = [&]() -> pot::coroutines::task<void>
        {
            auto g1 = co_await lock.lock();

            pot::sync::async_lock::scoped_lock_guard g2 = std::move(g1);

            pot::sync::async_lock::scoped_lock_guard g3 = std::move(g2);

            co_return;
        };

        move_test_task().get();

        auto check_task = [&]() -> pot::coroutines::task<bool>
        {
            auto g = co_await lock.lock();
            co_return true;
        }();

        REQUIRE(check_task.get() == true);
    }

    SECTION("Mutual exclusion check (Data Race Protection)")
    {
        pot::thread_pool_executor exec("lock_test_pool", 8);
        pot::sync::async_lock lock;

        int shared_counter = 0;
        const int iterations = 10000;

        auto run_race = [&]() -> pot::coroutines::lazy_task<void>
        {
            co_await pot::algorithms::parfor(exec, 0, iterations,
                                             [&](int) -> pot::coroutines::task<void>
                                             {
                                                 auto guard = co_await lock.lock();

                                                 int temp = shared_counter;
                                                 std::this_thread::yield();
                                                 shared_counter = temp + 1;

                                                 co_return;
                                             });
        };

        run_race().get();
        exec.shutdown();

        REQUIRE(shared_counter == iterations);
    }

    SECTION("Mutual exclusion with std::vector (Heavy corruption check)")
    {
        pot::thread_pool_executor exec("vector_test_pool", 8);
        pot::sync::async_lock lock;

        std::vector<int> shared_vec;
        const int iterations = 10000;

        auto run_vec_test = [&]() -> pot::coroutines::lazy_task<void>
        {
            co_await pot::algorithms::parfor(exec, 0, iterations,
                                             [&](int i) -> pot::coroutines::task<void>
                                             {
                                                 auto guard = co_await lock.lock();

                                                 shared_vec.push_back(i);
                                                 co_return;
                                             });
        };

        run_vec_test().get();
        exec.shutdown();

        REQUIRE(shared_vec.size() == iterations);
    }

    SECTION("Sequential consistency / FIFO approximation")
    {
        std::vector<int> execution_order;
        pot::sync::async_lock lock;
        int current_holder = 0;

        auto worker = [&](int id) -> pot::coroutines::task<void>
        {
            auto guard = co_await lock.lock();
            execution_order.push_back(id);
            co_return;
        };

        auto driver = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::coroutines::task<void>> tasks;
            auto g1 = co_await lock.lock();

            tasks.push_back(worker(1));
            tasks.push_back(worker(2));
            tasks.push_back(worker(3));
        };

        driver().get();

        auto check = [&]() -> pot::coroutines::task<void>
        {
            auto g = co_await lock.lock();
            co_return;
        }();
        check.get();
    }
}
