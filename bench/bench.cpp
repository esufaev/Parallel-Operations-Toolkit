#include <cinttypes>
#include <utility>
#include <chrono>
#include <future>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>

#include "../include/pot/experimental/thread_pool/thread_pool_gq_esu.h"
#include "../include/pot/experimental/thread_pool/thread_pool_lq_esu.h"

namespace pot::experimental
{
    template <typename DurationType, typename IterationCleanUpCallback, typename Func, typename... Args>
    DurationType time_it(size_t n, IterationCleanUpCallback &&callback, Func &&func, Args &&...args)
    {
        using clock_type = std::chrono::high_resolution_clock;

        clock_type::duration total_duration{0};

        for (size_t i = 0; i < n; ++i)
        {
            const auto start = clock_type::now();
            func(std::forward<Args>(args)...);
            const auto end = clock_type::now();
            total_duration += end - start;

            callback();
        }

        return std::chrono::duration_cast<DurationType>(total_duration / n);
    }

    template <int64_t static_chunk_size = -1, typename Executor, typename FuncType>
    void parfor(Executor &executor, int from, int to, FuncType &&func)
    {
        if (from >= to)
            return;

        const int64_t numIterations = to - from;
        int chunk_size = (static_chunk_size < 0) ? std::max(1ull, static_cast<unsigned long long int>(numIterations) / executor.thread_count()) : static_chunk_size;
        const int64_t numChunks = (numIterations + chunk_size - 1) / chunk_size;

        std::vector<std::future<void>> results;
        results.reserve(numChunks);

        for (int64_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
        {
            const int chunkStart = from + static_cast<int>(chunkIndex * chunk_size);
            const int chunkEnd = std::min(chunkStart + chunk_size, to);

            results.emplace_back(executor.add_task([start = chunkStart, end = chunkEnd, &func]
                                                   {
                for (int i = start; i < end; ++i)
                {
                    func(i);
                } }));
        }

        for (auto &result : results)
        {
            result.wait();
        }
    }
} // namespace pot::experimental

using namespace std::chrono;

template <typename QueueMode>
unsigned int task_1(int num_threads)
{
    constexpr auto vec_size = 1000000;
    constexpr auto experiment_count = 2;

    QueueMode pool(num_threads);

    std::vector<double> vec_a(vec_size, 1.0);
    std::vector<double> vec_b(vec_size, 2.0);
    std::vector<double> vec_c(vec_size);

    auto clear_c = [&vec_c]
    { std::fill(vec_c.begin(), vec_c.end(), 0.0); };

    clear_c();

    auto duration = pot::experimental::time_it<milliseconds>(experiment_count, [] {}, [&]
                                                             { pot::experimental::parfor(pool, 0, vec_a.size(), [&](const int i)
                                                                                         { vec_c[i] = vec_a[i] + vec_b[i]; }); });

    return duration.count();
}

void run_benchmark(const int numThreads = std::thread::hardware_concurrency())
{
    const int numIterations = 10;

    std::ofstream output_file("benchmark_results.dat", std::ios::binary);

    for (bool is_lq : {true, false})
    {
        for (int num_threads = 1; num_threads <= numThreads; num_threads++)
        {
            double total_duration = 0;
            for (int i = 0; i < numIterations; ++i)
            {
                if (is_lq)
                {
                    total_duration += task_1<pot::experimental::thread_pool::thread_pool_lq_esu>(num_threads);
                }
                else
                {
                    total_duration += task_1<pot::experimental::thread_pool::thread_pool_gq_esu>(num_threads);
                }
            }
            double avg_duration = total_duration / numIterations;

            std::string type = is_lq ? "LQ" : "GQ";
            std::cout << type << " Threads: " << num_threads << ", Avg Time: " << avg_duration << " ms" << std::endl;

            output_file.write(reinterpret_cast<const char *>(&num_threads), sizeof(num_threads));
            output_file.write(reinterpret_cast<const char *>(&avg_duration), sizeof(avg_duration));
            output_file.write(type.c_str(), type.size() + 1);
        }
    }
}

int main()
{
    run_benchmark();
    return 0;
}