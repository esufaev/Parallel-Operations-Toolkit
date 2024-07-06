#include <iostream>
#include <vector>
#include <future>
#include <chrono>
#include "../include/pot/experimental/thread_pool/bench.h"
#include "../include/pot/experimental/thread_pool/thread_pool_ll.h"

using namespace std::chrono;

void task_1()
{
    auto result = 1ull;
    for (auto i = 0ull; i < 8000000; i++) // Depends on the computer
        result *= i;
}

void run_benchmark_esu()
{
    const int num_tasks = 100;
    std::vector<int> thread_counts = {1, 2, 4, 8, 16, 32}; 

    std::vector<int> task_counts = {100, 200, 400, 800, 1000}; // Depends on the computer
    const int max_threads = 16;

    bench::graph_tt graph_threads;

    for (int num_threads : thread_counts)
    {
        pot::experimental::thread_pool::thread_pool pool(num_threads);

        auto start_time = high_resolution_clock::now();

        std::vector<std::future<void>> futures;
        for (int i = 0; i < num_tasks; ++i)
        {
            futures.push_back(pool.add_task(task_1));
        }

        for (auto &f : futures)
        {
            f.wait();
        }

        auto end_time = high_resolution_clock::now();
        double duration = duration_cast<milliseconds>(end_time - start_time).count();

        std::cout << "Threads: " << num_threads << ", Time: " << duration << " ms" << std::endl;
        graph_threads.add_point(duration, num_threads);
    }

    bench::graph_tn graph_tasks;

    for (int num_tasks : task_counts)
    {
        pot::experimental::thread_pool::thread_pool pool(max_threads);

        auto start_time = high_resolution_clock::now();

        std::vector<std::future<void>> futures;
        for (int i = 0; i < num_tasks; ++i)
        {
            futures.push_back(pool.add_task(task_1));
        }

        for (auto &f : futures)
        {
            f.wait();
        }

        auto end_time = high_resolution_clock::now();
        double duration = duration_cast<milliseconds>(end_time - start_time).count();

        std::cout << "Tasks: " << num_tasks << ", Time: " << duration << " ms" << std::endl;
        graph_tasks.add_point(duration, num_tasks); 
    }

    graph_threads.plot();
    graph_tasks.plot();
}

int main()
{
    run_benchmark_esu();
    return 0;
}
