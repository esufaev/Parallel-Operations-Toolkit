#include <iostream>
#include <vector>
#include <future>
#include <chrono>
#include <string>
#include <map>
#include "../include/pot/experimental/bench/bench.h"
#include "../include/pot/experimental/thread_pool/thread_pool_lq_esu.h"
#include "../include/pot/experimental/thread_pool/thread_pool_gq_esu.h"

using namespace std::chrono;

void task_1()
{
    auto result = 1ull;
    for (auto i = 0ull; i < 14000000; i++) // Depends on the computer
        result *= i;
}

void run_benchmark_esu()
{
    const int num_tasks = 800;
    std::vector<int> thread_counts = {4, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32};
    std::vector<int> task_counts = {100, 200, 400, 800, 1000, 1200, 1400}; // Depends on the computer
    const int max_threads = 16;

    bench::graph graph_threads;
    bench::graph graph_tasks;

    // Benchmark for thread_pool_lq_esu
    {
        for (int num_threads : thread_counts)
        {
            pot::experimental::thread_pool::thread_pool_lq_esu pool(num_threads);

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

            std::cout << "LQ Threads: " << num_threads << ", Time: " << duration << " ms" << std::endl;
            graph_threads.add_point("LQ", duration, num_threads);
        }

        for (int num_tasks : task_counts)
        {
            pot::experimental::thread_pool::thread_pool_lq_esu pool(max_threads);

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

            std::cout << "LQ Tasks: " << num_tasks << ", Time: " << duration << " ms" << std::endl;
            graph_tasks.add_point("LQ", duration, num_tasks);
        }
    }

    // Benchmark for thread_pool_gq_esu
    {
        for (int num_threads : thread_counts)
        {
            pot::experimental::thread_pool::thread_pool_gq_esu pool(num_threads);

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

            std::cout << "GQ Threads: " << num_threads << ", Time: " << duration << " ms" << std::endl;
            graph_threads.add_point("GQ", duration, num_threads);
        }

        for (int num_tasks : task_counts)
        {
            pot::experimental::thread_pool::thread_pool_gq_esu pool(max_threads);

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

            std::cout << "GQ Tasks: " << num_tasks << ", Time: " << duration << " ms" << std::endl;
            graph_tasks.add_point("GQ", duration, num_tasks);
        }
    }

    graph_threads.plot();
    graph_tasks.plot();
}

int main()
{
    run_benchmark_esu();
    return 0;
}
