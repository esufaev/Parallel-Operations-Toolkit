#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "pot/algorithms/parfor.h"
#include "pot/sandbox/thread_pool_executor.h"
#include "pot/sync/async_lock.h"
#include "pot/utils/time_it.h"

#include <Eigen/Sparse>
#include <cmath>
#include <fmt/core.h>
#include <memory>
#include <thread>
#include <vector>

constexpr int MANDEL_WIDTH = 2000;
constexpr int MANDEL_HEIGHT = 2000;
constexpr int MANDEL_NUM_BINS = 64;

int compute_mandelbrot(double c_re, double c_im, int max_iter)
{
    double z_re = 0.0, z_im = 0.0;
    int iter = 0;
    while (z_re * z_re + z_im * z_im <= 4.0 && iter < max_iter)
    {
        double z_re_new = z_re * z_re - z_im * z_im + c_re;
        z_im = 2.0 * z_re * z_im + c_im;
        z_re = z_re_new;
        iter++;
    }
    return iter;
}

template <typename ExecutorType>
void build_mandelbrot_pot_get(int64_t max_iter, std::shared_ptr<ExecutorType> executor)
{
    std::pmr::synchronized_pool_resource pool(std::pmr::new_delete_resource());
    pot::memory::set_memory_resource(&pool);

    std::vector<pot::sync::async_lock> bin_locks(MANDEL_NUM_BINS);
    std::vector<int64_t> histogram(MANDEL_NUM_BINS, 0);

    pot::algorithms::parfor(*executor, (int64_t)0, (int64_t)MANDEL_HEIGHT,
                            [&](int64_t y) -> pot::coroutines::task<void>
                            {
                                double c_im = -1.5 + 3.0 * y / MANDEL_HEIGHT;
                                for (int x = 0; x < MANDEL_WIDTH; ++x)
                                {
                                    double c_re = -2.0 + 3.0 * x / MANDEL_WIDTH;
                                    int iter = compute_mandelbrot(c_re, c_im, max_iter);
                                    int bin = (iter * (MANDEL_NUM_BINS - 1)) / max_iter;

                                    auto _ = co_await bin_locks[bin].lock(&*executor);
                                    histogram[bin]++;
                                }
                            })
        .get(executor.get()); 
}

template <typename ExecutorType>
void build_mandelbrot_pot_blocking_get(int64_t max_iter, std::shared_ptr<ExecutorType> executor)
{
    std::pmr::synchronized_pool_resource pool(std::pmr::new_delete_resource());
    pot::memory::set_memory_resource(&pool);

    std::vector<pot::sync::async_lock> bin_locks(MANDEL_NUM_BINS);
    std::vector<int64_t> histogram(MANDEL_NUM_BINS, 0);

    pot::algorithms::parfor(*executor, (int64_t)0, (int64_t)MANDEL_HEIGHT,
                            [&](int64_t y) -> pot::coroutines::task<void>
                            {
                                double c_im = -1.5 + 3.0 * y / MANDEL_HEIGHT;
                                for (int x = 0; x < MANDEL_WIDTH; ++x)
                                {
                                    double c_re = -2.0 + 3.0 * x / MANDEL_WIDTH;
                                    int iter = compute_mandelbrot(c_re, c_im, max_iter);
                                    int bin = (iter * (MANDEL_NUM_BINS - 1)) / max_iter;

                                    auto _ = co_await bin_locks[bin].lock(&*executor);
                                    histogram[bin]++;
                                }
                            })
        .blocking_get(); 
}

TEST_CASE("Compare get() vs blocking_get() on LQLF_Steal_Seq", "[benchmark]")
{
    const std::vector<int64_t> thread_counts = { (int64_t)std::thread::hardware_concurrency() };
    const std::vector<int64_t> max_iters = { 2000, 4000, 6000, 8000, 10000, 50000, 80000 };
    const size_t test_runs = 5;

    pot::executor::cpu_set();

    for (int core : pot::coroutines::details::task_meta::sorted_cpu_list) 
    {
      std::cout << "Core ID: " << core << "\n";
    }

    SECTION("Mandelbrot Fractal: get() vs blocking_get()")
    {
        fmt::print("\n=== LQLF_Steal_Seq: get() vs blocking_get() ===\n");
        fmt::print("{:>8} {:>10} | {:>15} {:>15} | {:>10}\n", 
                   "Threads", "MaxIter", "get() (Steal)", "blocking_get()", "Diff (%)");
        fmt::print("{:-<65}\n", "");

        for (auto thread_count : thread_counts)
        {
            auto exec_lqlf_seq = std::make_shared<pot::executors::thread_pool_executor_lqlf_steal_seq>("LQLFSeq", thread_count);

            for (auto param : max_iters)
            {
                auto t_get = pot::utils::time_it<std::chrono::duration<double>>(
                    test_runs, []() {}, build_mandelbrot_pot_get<pot::executors::thread_pool_executor_lqlf_steal_seq>, param, exec_lqlf_seq);

                auto t_blocking_get = pot::utils::time_it<std::chrono::duration<double>>(
                    test_runs, []() {}, build_mandelbrot_pot_blocking_get<pot::executors::thread_pool_executor_lqlf_steal_seq>, param, exec_lqlf_seq);

                double diff_percent = ((t_blocking_get.count() - t_get.count()) / t_blocking_get.count()) * 100.0;

                fmt::print("{:8} {:10} | {:15.5f} {:15.5f} | {:+9.2f}%\n",
                           thread_count, param, t_get.count(), t_blocking_get.count(), diff_percent);
            }
        }
    }
}
