#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <future>
#include "../include/pot/executors/thread_pool_executor.h"
#include "plot_interface.h"

void task()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

template<typename Executor>
double measure_task_duration(Executor &pool, int task_count)
{
    std::vector<std::future<void>> futures;
    futures.reserve(task_count);

    auto start = std::chrono::system_clock::now();
    for (int i = 0; i < task_count; i++)
    {
        futures.push_back(pool.run([&]()
                                   { task(); }));
    }

    for (auto &future : futures)
    {
        future.get();
    }
    auto end = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main()
{
    constexpr auto experiment_count = 50;
    constexpr auto task_count = 50;

    bench::graph_threads graph;

    for (unsigned int num_threads = 1; num_threads < std::thread::hardware_concurrency(); num_threads++)
    {
        pot::executors::thread_pool_executor_lq pool_gq("GQ", num_threads);
        pot::executors::thread_pool_executor_lq pool_lq("LQ", num_threads);

        double average_dur_gq = 0.0;
        double average_dur_lq = 0.0;

        for (int i = 0; i < experiment_count; i++)
        {
            std::cout << "Started " << num_threads << std::endl;

            average_dur_gq += measure_task_duration(pool_gq, task_count);
            average_dur_lq += measure_task_duration(pool_lq, task_count);

            std::cout << "Finished " << num_threads << std::endl;
        }

        average_dur_gq /= experiment_count;
        average_dur_lq /= experiment_count;

        graph.add_first_point(num_threads, average_dur_gq);
        graph.add_second_point(num_threads, average_dur_lq);
    }

    graph.plot();

    return 0;
}
