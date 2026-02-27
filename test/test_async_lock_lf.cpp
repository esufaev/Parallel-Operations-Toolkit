#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <Eigen/Sparse>
#include <fmt/core.h>
#include <memory>
#include <vector>
#include <chrono>


#include "pot/algorithms/parfor.h"
#include "pot/executors/exp/thread_pool_executor_exp.h" 
#include "pot/sync/async_lock.h"
#include "pot/sync/async_lock_m.h"
#include "pot/utils/time_it.h"


std::tuple<double, double, double> calculation(const int64_t row_idx)
{
    double vl = 0.0, vc = 0.0, vr = 0.0;
    for (auto i = 0; i < 100; ++i)
    {
        double val = static_cast<double>(row_idx) * 10.0;
        vl += -std::exp(std::sin(std::cos(val))) - std::sin(10);
        vc += std::exp(std::sin(std::cos(val)));
        vr += -std::exp(std::sin(std::cos(val))) + std::sin(10);
    }
    return {vl, vc, vr};
}


template <typename Executor, typename LockType> 
void buildA_pot(const int64_t n, std::shared_ptr<Executor> executor)
{
    std::pmr::synchronized_pool_resource pool(std::pmr::new_delete_resource());
    pot::memory::set_memory_resource(&pool);

    Eigen::SparseMatrix<double> A(n, n);
    A.resize(n, n);

    LockType locker; 

    pot::algorithms::parfor(*executor, (int64_t)0, n,
                            [&](int64_t i) -> pot::coroutines::task<void>
                            {
                                auto [vl, vc, vr] = calculation(i);
                                auto _ = co_await locker.lock(&*executor);

                                if (i > 0)
                                    A.coeffRef(i, i - 1) = vl;
                                A.coeffRef(i, i) = vc;
                                if (i < n - 1)
                                    A.coeffRef(i, i + 1) = vr;
                            })
        .get();
}

TEST_CASE("Async Lock Performance: Std Mutex vs Lock-Free", "[benchmark]")
{
    
    const std::vector<int64_t> thread_counts = {8};
    const std::vector<int64_t> sizes = {10'000, 20'000, 30'000, 40'000, 50'000, 60'000};
    const size_t test_count = 3;

    SECTION("Compare locks on GQ Executor")
    {
        
        fmt::print("{:>7} {:>7} | {:>12} {:>12}\n", 
                   "Threads", "Size", "Std Mutex", "Lock-Free");
        fmt::print("{:-<45}\n", "");

        for (auto thread_count : thread_counts)
        {
            
            auto exec_gq = std::make_shared<pot::executors::thread_pool_executor_gq>("GQ", thread_count);

            for (auto n : sizes)
            {
                
                auto time_lock_std = pot::utils::time_it<std::chrono::duration<double>>(
                    test_count, 
                    []() {}, 
                    buildA_pot<pot::executors::thread_pool_executor_gq, pot::sync::async_lock_opt>, 
                    n, exec_gq);

                
                auto time_lock_lf = pot::utils::time_it<std::chrono::duration<double>>(
                    test_count, 
                    []() {}, 
                    buildA_pot<pot::executors::thread_pool_executor_gq, pot::sync::async_lock_lf>, 
                    n, exec_gq);

                
                fmt::print("{:7} {:7} | {:12.5f} {:12.5f}\n",
                           thread_count, n, 
                           time_lock_std.count(), time_lock_lf.count());
            }
        }
    }
}
