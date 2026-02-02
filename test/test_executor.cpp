#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <thread>

#include "pot/executors/thread_pool_executor.h"
#include "pot/coroutines/when_all.h"


int plain_function(int a, int b)
{
    return a + b;
}

pot::coroutines::task<int> coro_function(int a)
{
    co_return a * 2;
}

pot::coroutines::lazy_task<int> lazy_coro_function(int a)
{
    co_return a * 3;
}

pot::thread_pool_executor pool("test_pool");

TEST_CASE("thread_pool_executor tests")
{
    SECTION("run - lambda (void)")
    {
        std::atomic<bool> executed{false};
        std::thread::id main_thread_id = std::this_thread::get_id();
        std::thread::id worker_thread_id;

        auto t = pool.run([&]() {
            executed = true;
            worker_thread_id = std::this_thread::get_id();
        });

        t.get();

        REQUIRE(executed.load());
        REQUIRE(worker_thread_id != main_thread_id);
    }

    SECTION("run - lambda (return value)")
    {
        auto t = pool.run([]() {
            return 42;
        });

        int result = t.get();
        REQUIRE(result == 42);
    }

    SECTION("run - capture lambda")
    {
        int value = 10;
        auto t1 = pool.run([value]() {
            return value * 2;
        });
        REQUIRE(t1.get() == 20);

        std::atomic<int> shared_val{5};
        auto t2 = pool.run([&shared_val]() {
            shared_val.fetch_add(5);
        });
        t2.get();
        REQUIRE(shared_val.load() == 10);
    }

    SECTION("run - lambda returning task (nested coroutine)")
    {
        auto t = pool.run([]() -> pot::coroutines::task<int> {
            co_return 100;
        });

        int result = t.get();
        REQUIRE(result == 100);
    }

    SECTION("run - lambda returning lazy_task")
    {
        auto t = pool.run([]() -> pot::coroutines::lazy_task<int> {
            co_return 200;
        });

        int result = t.get();
        REQUIRE(result == 200);
    }

    SECTION("run - passing arguments to lambda")
    {
        auto t = pool.run([](int a, int b) {
            return a + b;
        }, 10, 20);

        REQUIRE(t.get() == 30);
    }

    SECTION("run - free functions")
    {
        auto t1 = pool.run(plain_function, 5, 7);
        REQUIRE(t1.get() == 12);

        auto t2 = pool.run(coro_function, 10);
        REQUIRE(t2.get() == 20);

        auto t3 = pool.run(lazy_coro_function, 10);
        REQUIRE(t3.get() == 30);
    }
    
    SECTION("lazy_run - basic test")
    {
        std::atomic<int> counter{0};
        
        auto lazy_t = pool.lazy_run([&]() {
            counter++;
            return 1;
        });

        REQUIRE(counter.load() == 0); 
        
        lazy_t.get(); 
        
        REQUIRE(counter.load() == 1);
    }

    SECTION("complex chain - run inside run")
    {
        auto t = pool.run([&]() -> pot::coroutines::task<int> {
            auto inner_task = pool.run([]() { return 77; });
            co_return co_await inner_task;
        });

        REQUIRE(t.get() == 77);
    }
}

TEST_CASE("co_await integration")
{
    SECTION("co_await on run() result (lambda with return)")
    {
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            auto task_in_pool = pool.run([]() { return 123; });
            
            int result = co_await task_in_pool;
            
            REQUIRE(result == 123);
            co_return;
        };

        root_task().get();
    }

    SECTION("co_await on lazy_run() result (lambda with return)")
    {
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            auto lazy_task_in_pool = pool.lazy_run([]() { return 456; });
            
            int result = co_await lazy_task_in_pool;
            
            REQUIRE(result == 456);
            co_return;
        };

        root_task().get();
    }

    SECTION("co_await on run() result (capture lambda)")
    {
        int factor = 2;
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            auto task_in_pool = pool.run([factor](int input) { 
                return input * factor; 
            }, 50);
            
            int result = co_await task_in_pool;
            REQUIRE(result == 100);
            co_return;
        };

        root_task().get();
    }

    SECTION("co_await on run() result (free function)")
    {
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            auto task_in_pool = pool.run(plain_function, 10, 20);
            
            int result = co_await task_in_pool;
            REQUIRE(result == 30);
            co_return;
        };

        root_task().get();
    }

    SECTION("co_await on run() result (coroutine lambda inside)")
    {
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            auto task_in_pool = pool.run([]() -> pot::coroutines::task<int> {
                co_return 999;
            });

            int result = co_await task_in_pool;
            REQUIRE(result == 999);
            co_return;
        };

        root_task().get();
    }

    SECTION("Nested execution: pool thread awaits another pool task")
    {
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            auto task_A = pool.run([&]() -> pot::coroutines::task<int> {
                
                std::thread::id thread_A = std::this_thread::get_id();
                
                auto task_B = pool.run([]() {
                    return 42;
                });

                int val = co_await task_B;
                
                co_return val + 1;
            });

            int result = co_await task_A;
            REQUIRE(result == 43);
            co_return;
        };

        root_task().get();
    }
    
    SECTION("co_await on run() (void return)")
    {
        std::atomic<bool> flag{false};
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            auto void_task = pool.run([&]() {
                flag = true;
            });

            co_await void_task;
            
            REQUIRE(flag.load());
            co_return;
        };
        
        root_task().get();
    }
}

TEST_CASE("Vector of tasks operations")
{
    const int task_count = 10;

    SECTION("std::vector of tasks + .get() iteration")
    {
        std::vector<pot::coroutines::task<int>> tasks;
        tasks.reserve(task_count);

        for (int i = 0; i < task_count; ++i)
        {
            tasks.emplace_back(pool.run([i]() {
                return i;
            }));
        }

        int sum = 0;
        for (auto &t : tasks)
        {
            sum += t.get();
        }

        REQUIRE(sum == 45);
    }

    SECTION("std::vector of tasks + co_await iteration")
    {
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::coroutines::task<int>> tasks;
            tasks.reserve(task_count);

            for (int i = 0; i < task_count; ++i)
            {
                tasks.emplace_back(pool.run([i]() {
                    return i * 2;
                }));
            }

            int sum = 0;
            for (auto &t : tasks)
            {
                sum += co_await std::move(t);
            }

            REQUIRE(sum == 90); 
            co_return;
        };

        root_task().get();
    }

    SECTION("std::vector of lazy_tasks + co_await iteration")
    {
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::coroutines::lazy_task<int>> tasks;
            tasks.reserve(task_count);

            for (int i = 0; i < task_count; ++i)
            {
                tasks.emplace_back(pool.lazy_run([i]() {
                    return 100 + i;
                }));
            }

            int sum = 0;
            for (auto &t : tasks)
            {
                sum += co_await std::move(t);
            }

            REQUIRE(sum == 1045);
            co_return;
        };

        root_task().get();
    }

    SECTION("std::vector of void tasks (side effects check)")
    {
        std::atomic<int> counter{0};
        
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::coroutines::task<void>> tasks;
            tasks.reserve(task_count);

            for (int i = 0; i < task_count; ++i)
            {
                tasks.emplace_back(pool.run([&]() {
                    counter.fetch_add(1, std::memory_order_relaxed);
                }));
            }

            for (auto &t : tasks)
            {
                co_await std::move(t);
            }
            
            co_return;
        };

        root_task().get();
        REQUIRE(counter.load() == task_count);
    }

    SECTION("Dynamic vector filling inside a pool task")
    {
        auto main_task = pool.run([&, task_count]() -> pot::coroutines::task<int> {
            
            std::vector<pot::coroutines::task<int>> subtasks;
            
            for(int i = 0; i < task_count; ++i) {
                subtasks.push_back(pool.run([i]() { return i; }));
            }
            
            int local_sum = 0;
            for(auto& t : subtasks) {
                local_sum += co_await std::move(t);
            }
            
            co_return local_sum;
        });

        REQUIRE(main_task.get() == 45);
    }
    
    SECTION("std::vector with captures and move-only types handling")
    {
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::coroutines::task<int>> tasks;
            int base = 10;

            for (int i = 0; i < 5; ++i)
            {
                tasks.emplace_back(pool.run([base, i]() {
                    return base + i;

                }));
            }

            int sum = 0;
            for (auto &t : tasks)
            {
                sum += co_await std::move(t);
            }

            REQUIRE(sum == 60);
            co_return;
        };

        root_task().get();
    }
}


TEST_CASE("when_all integration tests")
{
    SECTION("when_all(vector) - Eager tasks (pool.run)")
    {
        std::atomic<int> counter{0};
        const int task_count = 20;

        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::coroutines::task<void>> tasks;
            tasks.reserve(task_count);

            for (int i = 0; i < task_count; ++i)
            {
                tasks.emplace_back(pool.run([&counter]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    counter.fetch_add(1, std::memory_order_relaxed);
                }));
            }

            co_await pot::coroutines::when_all(tasks);
            co_return;
        };

        root_task().get();

        REQUIRE(counter.load() == task_count);
    }

    SECTION("when_all(iterators) - Eager tasks")
    {
        std::atomic<int> sum{0};
        const int task_count = 10;

        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::coroutines::task<int>> tasks;
            tasks.reserve(task_count);

            for (int i = 0; i < task_count; ++i)
            {
                tasks.emplace_back(pool.run([i]() {
                    return 1; 
                }));
            }

            co_await pot::coroutines::when_all(tasks.begin(), tasks.end());

            co_return;
        };

        std::atomic<int> check_counter{0};
        auto root_check = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::coroutines::task<void>> tasks;
            for(int i=0; i<10; ++i) {
                tasks.emplace_back(pool.run([&check_counter](){
                    check_counter++;
                }));
            }

            co_await pot::coroutines::when_all(tasks.begin(), tasks.end());
            co_return;
        };

        root_check().get();
        REQUIRE(check_counter.load() == 10);
    }

    SECTION("when_all(variadic) - Mixed tasks")
    {
        std::atomic<bool> f1{false};
        std::atomic<bool> f2{false};
        std::atomic<bool> f3{false};

        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            auto t1 = pool.run([&]() { 
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                f1 = true; 
            });

            auto t2 = pool.run([&]() { 
                f2 = true; 
            });

            
            auto t3 = pool.run([&]() -> int { 
                f3 = true; 
                return 42; 
            });

            
            co_await pot::coroutines::when_all(std::move(t1), std::move(t2), std::move(t3));

            REQUIRE(f1.load());
            REQUIRE(f2.load());
            REQUIRE(f3.load());

            co_return;
        };

        root_task().get();
    }

    SECTION("when_all with lazy_run (Sequential Execution Warning)")
    {
        std::atomic<int> counter{0};

        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            std::vector<pot::coroutines::lazy_task<void>> tasks;
            for(int i=0; i<5; ++i) {
                tasks.emplace_back(pool.lazy_run([&counter](){
                            counter++;
                }));
            }

            co_await pot::coroutines::when_all(tasks);

            REQUIRE(counter.load() == 5);
            co_return;
        };

        root_task().get();
    }

    SECTION("when_all exception safety (Basic check)")
    {
        auto root_task = [&]() -> pot::coroutines::task<void>
        {
            auto t1 = pool.run([](){ return 1; });
            auto t2 = pool.run([](){ 
                throw std::runtime_error("Oops"); 
                return 2;
            });
            auto t3 = pool.run([](){ return 3; });

            co_await pot::coroutines::when_all(std::move(t1), std::move(t2), std::move(t3));
            co_return;
        };

        REQUIRE_THROWS_AS(root_task().get(), std::runtime_error);
    }
}
