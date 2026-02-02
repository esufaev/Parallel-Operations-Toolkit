#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "pot/coroutines/task.h"

pot::coroutines::lazy_task<int> fibonacci_lazy(int i)
{
    if (i == 0)
        co_return 0;
    if (i == 1)
        co_return 1;

    auto fib1 = fibonacci_lazy(i - 1);
    auto fib2 = fibonacci_lazy(i - 2);

    co_return (co_await fib1) + (co_await fib2);
}

TEST_CASE("lazy_task test")
{
    SECTION("lazy_task - lambda co_return")
    {
        auto task1 = []() -> pot::coroutines::lazy_task<void>
        {
            std::printf("LAZY_TASK - reg lambda\n");
            co_return;
        };

        task1().get();

        auto task2 = []() -> pot::coroutines::lazy_task<void>
        {
            std::printf("LAZY_TASK - reg lambda\n");
            co_return;
        }();

        task2.get();

        auto task3 = []() -> pot::coroutines::lazy_task<int>
        {
            std::printf("LAZY_TASK - reg lambda\n");
            co_return 42;
        };

        REQUIRE(task3().get() == 42);

        auto task4 = []() -> pot::coroutines::lazy_task<int>
        {
            std::printf("LAZY_TASK - reg lambda\n");
            co_return 42;
        }();

        REQUIRE(task4.get() == 42);
    }

    SECTION("lazy_task - capt lambda co_return")
    {
        std::atomic<int> val = 1;
        auto task1 = [&val]() -> pot::coroutines::lazy_task<void>
        {
            val.fetch_add(41);
            co_return;
        };

        task1().get();

        REQUIRE(val.load() == 42);

        auto task2 = [&val]() -> pot::coroutines::lazy_task<int>
        {
            val.fetch_add(41);
            co_return val.load();
        };

        REQUIRE(task2().get() == 83);
    }

    SECTION("lazy_task - lambda co_await")
    {
        auto await_void_func = []() -> pot::coroutines::lazy_task<void>
        {
            std::printf("await_void_func\n");
            co_return;
        };
        auto await_ret_func = []() -> pot::coroutines::lazy_task<int>
        {
            std::printf("await_ret_func\n");
            co_return 42;
        };

        auto task1 = [&]() -> pot::coroutines::lazy_task<void>
        {
            co_await await_void_func();
            co_return;
        };

        task1().get();

        auto task2 = [&]() -> pot::coroutines::lazy_task<int>
        {
            int result = co_await await_ret_func();
            std::printf("HUIPIZDA\n");
            co_return result * 2;
        };

        auto result = task2().get();

        REQUIRE(result == 84);
    }

    SECTION("lazy_task - capt lambda co_await")
    {
        int capt_value = 42;

        auto capt_await_void_func = [&]() -> pot::coroutines::lazy_task<void>
        {
            std::printf("capt_awaait_void_func\n");
            std::printf("capt_value = %d\n", capt_value);
            co_return;
        };
        auto capt_await_ret_func = [&]() -> pot::coroutines::lazy_task<int>
        {
            std::printf("capt_await_ret_func\n");
            co_return 42 * capt_value;
        };

        auto task1 = [&]() -> pot::coroutines::lazy_task<void>
        {
            co_await capt_await_void_func();
            co_return;
        };

        task1().get();

        auto task2 = [&]() -> pot::coroutines::lazy_task<int>
        {
            int result = co_await capt_await_ret_func();
            co_return result + 1;
        };

        REQUIRE(task2().get() == (42 * 42 + 1));
    }

    SECTION("lazy_task - fibonacci")
    {
        auto task_res = [&]() -> pot::coroutines::lazy_task<int>
        {
            int result = co_await fibonacci_lazy(19);
            co_return result;
        };

        int result = task_res().get();
        printf("FIBONACCI (lazy): %d\n", result);
        REQUIRE(result == 4181);
    }

    SECTION("lazy_task - long for loop")
    {
        std::atomic<int> counter{0};

        auto work = [&counter]() -> pot::coroutines::lazy_task<void>
        {
            for (int i = 0; i < 1000; ++i)
            {
                co_await [&]() -> pot::coroutines::lazy_task<void>
                {
                    counter.fetch_add(1, std::memory_order_relaxed);
                    co_return;
                }();
            }
            co_return;
        };

        work().get();
        REQUIRE(counter.load(std::memory_order_relaxed) == 1000);
    }

    SECTION("lazy_task - vector + for (get version)")
    {
        std::vector<pot::coroutines::lazy_task<int>> tasks;
        tasks.reserve(10);

        for (int i = 0; i < 10; ++i)
        {
            tasks.emplace_back([](int i) -> pot::coroutines::lazy_task<int>
                               { co_return i * 2; }(i));
        }

        int sum = 0;

        for (auto &t : tasks)
        {
            sum += t.get();
        }

        REQUIRE(sum == 90);
    }

    SECTION("lazy_task - vector + for (co_await version)")
    {
        std::vector<pot::coroutines::lazy_task<int>> tasks;
        tasks.reserve(10);

        for (int i = 0; i < 10; ++i)
        {
            tasks.emplace_back([](int i) -> pot::coroutines::lazy_task<int>
                               { co_return i * 2; }(i));
        }

        auto sum_task = [&]() -> pot::coroutines::lazy_task<int>
        {
            int sum = 0;
            for (auto &t : tasks)
            {

                sum += co_await std::move(t);
            }
            co_return sum;
        };

        int result = sum_task().get();

        REQUIRE(result == 90);
    }
}

pot::coroutines::task<int> fibonacci_task(int i)
{
    if (i == 0)
        co_return 0;
    if (i == 1)
        co_return 1;

    auto fib1 = fibonacci_task(i - 1);
    auto fib2 = fibonacci_task(i - 2);

    co_return (co_await fib1) + (co_await fib2);
}

TEST_CASE("task test")
{
    SECTION("task - lambda co_return")
    {
        auto task1 = []() -> pot::coroutines::task<void>
        {
            std::printf("TASK - reg lambda\n");
            co_return;
        };

        task1().get();

        auto task2 = []() -> pot::coroutines::task<void>
        {
            std::printf("TASK - reg lambda\n");
            co_return;
        }();

        task2.get();

        auto task3 = []() -> pot::coroutines::task<int>
        {
            std::printf("TASK - reg lambda\n");
            co_return 42;
        };

        REQUIRE(task3().get() == 42);

        auto task4 = []() -> pot::coroutines::task<int>
        {
            std::printf("TASK - reg lambda\n");
            co_return 42;
        }();

        REQUIRE(task4.get() == 42);
    }

    SECTION("task - capt lambda co_return")
    {
        std::atomic<int> val = 1;
        auto task1 = [&val]() -> pot::coroutines::task<void>
        {
            val.fetch_add(41);
            co_return;
        };

        task1().get();

        REQUIRE(val.load() == 42);

        auto task2 = [&val]() -> pot::coroutines::task<int>
        {
            val.fetch_add(41);
            co_return val.load();
        };

        REQUIRE(task2().get() == 83);
    }

    SECTION("task - lambda co_await")
    {
        auto await_void_func = []() -> pot::coroutines::task<void>
        {
            std::printf("await_void_func\n");
            co_return;
        };
        auto await_ret_func = []() -> pot::coroutines::task<int>
        {
            std::printf("await_ret_func\n");
            co_return 42;
        };

        auto task1 = [&]() -> pot::coroutines::task<void>
        {
            co_await await_void_func();
            co_return;
        };

        task1().get();

        auto task2 = [&]() -> pot::coroutines::task<int>
        {
            int result = co_await await_ret_func();
            std::printf("HUIPIZDA\n");
            co_return result * 2;
        };

        auto result = task2().get();

        REQUIRE(result == 84);
    }

    SECTION("task - capt lambda co_await")
    {
        int capt_value = 42;

        auto capt_await_void_func = [&]() -> pot::coroutines::task<void>
        {
            std::printf("capt_awaait_void_func\n");
            std::printf("capt_value = %d\n", capt_value);
            co_return;
        };
        auto capt_await_ret_func = [&]() -> pot::coroutines::task<int>
        {
            std::printf("capt_await_ret_func\n");
            co_return 42 * capt_value;
        };

        auto task1 = [&]() -> pot::coroutines::task<void>
        {
            co_await capt_await_void_func();
            co_return;
        };

        task1().get();

        auto task2 = [&]() -> pot::coroutines::task<int>
        {
            int result = co_await capt_await_ret_func();
            co_return result + 1;
        };

        REQUIRE(task2().get() == (42 * 42 + 1));
    }

    SECTION("task - fibonacci")
    {
        auto task_res = [&]() -> pot::coroutines::task<int>
        {
            int result = co_await fibonacci_task(19);
            co_return result;
        };

        int result = task_res().get();
        printf("FIBONACCI (task): %d\n", result);
        REQUIRE(result == 4181);
    }

    SECTION("task - long for loop")
    {
        std::atomic<int> counter{0};

        auto work = [&counter]() -> pot::coroutines::task<void>
        {
            for (int i = 0; i < 1000; ++i)
            {
                co_await [&]() -> pot::coroutines::task<void>
                {
                    counter.fetch_add(1, std::memory_order_relaxed);
                    co_return;
                }();
            }
            co_return;
        };

        work().get();
        REQUIRE(counter.load(std::memory_order_relaxed) == 1000);
    }

    SECTION("task - vector + for (get version)")
    {
        std::vector<pot::coroutines::task<int>> tasks;
        tasks.reserve(10);

        for (int i = 0; i < 10; ++i)
        {
            tasks.emplace_back([](int i) -> pot::coroutines::task<int> { co_return i * 2; }(i));
        }

        int sum = 0;

        for (auto &t : tasks)
        {
            sum += t.get();
        }

        REQUIRE(sum == 90);
    }

    SECTION("task - vector + for (co_await version)")
    {
        std::vector<pot::coroutines::task<int>> tasks;
        tasks.reserve(10);

        for (int i = 0; i < 10; ++i)
        {
            tasks.emplace_back([](int i) -> pot::coroutines::task<int> { co_return i * 2; }(i));
        }

        auto sum_task = [&]() -> pot::coroutines::task<int>
        {
            int sum = 0;
            for (auto &t : tasks)
            {

                sum += co_await std::move(t);
            }
            co_return sum;
        };

        int result = sum_task().get();

        REQUIRE(result == 90);
    }
}
