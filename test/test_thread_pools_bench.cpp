#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "pot/algorithms/parfor.h"
#include "pot/sandbox/thread_pool_executor.h"
#include "pot/sync/async_lock.h"
#include "pot/utils/time_it.h"

#include <Eigen/Sparse>
#include <chrono>
#include <cmath>
#include <fmt/core.h>
#include <latch>
#include <memory>
#include <thread>
#include <vector>

template <typename ExecutorType>
void run_high_contention_microtasks(int64_t total_tasks, std::shared_ptr<ExecutorType> executor)
{
	int64_t num_spawners = std::thread::hardware_concurrency();
	int64_t tasks_per_spawner = total_tasks / num_spawners;

	struct shared_state
	{
		std::latch completion;
		explicit shared_state(int64_t count) : completion(count)
		{
		}
	};

	std::vector<std::unique_ptr<shared_state>> states;
	states.reserve(num_spawners);
	for (int64_t i = 0; i < num_spawners; ++i)
	{
		states.push_back(std::make_unique<shared_state>(tasks_per_spawner));
	}

	pot::algorithms::parfor(*executor, (int64_t)0, num_spawners,
							[&](int64_t i)
							{
								auto &latch = states[i]->completion;
								for (int64_t j = 0; j < tasks_per_spawner; ++j)
								{
									executor->run_detached(
										[&latch]()
										{
											volatile int x = 0;
											x = x + 1;
											latch.count_down();
										});
								}
							})
		.get();

	for (auto &state : states)
		state->completion.wait();
}

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

template <typename ExecutorType> void build_mandelbrot_pot(int64_t max_iter, std::shared_ptr<ExecutorType> executor)
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
		.get();
}

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

template <typename ExecutorType> void buildA_pot(int64_t n, std::shared_ptr<ExecutorType> executor)
{
	std::pmr::synchronized_pool_resource pool(std::pmr::new_delete_resource());
	pot::memory::set_memory_resource(&pool);

	Eigen::SparseMatrix<double> A(n, n);
	A.resize(n, n);
	pot::sync::async_lock locker;

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

#define INIT_EXECUTORS(THREADS)                                                                                        \
	auto exec_gq = std::make_shared<pot::executors::thread_pool_executor_gq>("GQ", THREADS);                           \
	auto exec_lq = std::make_shared<pot::executors::thread_pool_executor_lq>("LQ", THREADS);                           \
	auto exec_lq_seq = std::make_shared<pot::executors::thread_pool_executor_lq_steal_seq>("LQSeq", THREADS);          \
	auto exec_lq_neigh = std::make_shared<pot::executors::thread_pool_executor_lq_steal_neighbor>("LQNeigh", THREADS); \
	auto exec_gs = std::make_shared<pot::executors::thread_pool_executor_gs>("GS", THREADS);                           \
	auto exec_ls = std::make_shared<pot::executors::thread_pool_executor_ls>("LS", THREADS);                           \
	auto exec_gs_hot = std::make_shared<pot::executors::thread_pool_executor_gs_hot>("GSHot", THREADS);                \
	auto exec_ls_hot = std::make_shared<pot::executors::thread_pool_executor_ls_hot>("LSHot", THREADS);                \
	auto exec_gpq = std::make_shared<pot::executors::thread_pool_executor_gpq>("GPQ", THREADS);

#define RUN_BENCHMARK_SUITE(WORKLOAD_TEMPLATE, PARAM)                                                                  \
	auto t_gq = pot::utils::time_it<std::chrono::duration<double>>(                                                    \
		test_runs, []() {}, WORKLOAD_TEMPLATE<pot::executors::thread_pool_executor_gq>, PARAM, exec_gq);               \
	auto t_lq = pot::utils::time_it<std::chrono::duration<double>>(                                                    \
		test_runs, []() {}, WORKLOAD_TEMPLATE<pot::executors::thread_pool_executor_lq>, PARAM, exec_lq);               \
	auto t_lq_seq = pot::utils::time_it<std::chrono::duration<double>>(                                                \
		test_runs, []() {}, WORKLOAD_TEMPLATE<pot::executors::thread_pool_executor_lq_steal_seq>, PARAM, exec_lq_seq); \
	auto t_lq_neigh = pot::utils::time_it<std::chrono::duration<double>>(                                              \
		test_runs, []() {}, WORKLOAD_TEMPLATE<pot::executors::thread_pool_executor_lq_steal_neighbor>, PARAM,          \
		exec_lq_neigh);                                                                                                \
	auto t_gs = pot::utils::time_it<std::chrono::duration<double>>(                                                    \
		test_runs, []() {}, WORKLOAD_TEMPLATE<pot::executors::thread_pool_executor_gs>, PARAM, exec_gs);               \
	auto t_ls = pot::utils::time_it<std::chrono::duration<double>>(                                                    \
		test_runs, []() {}, WORKLOAD_TEMPLATE<pot::executors::thread_pool_executor_ls>, PARAM, exec_ls);               \
	auto t_gs_hot = pot::utils::time_it<std::chrono::duration<double>>(                                                \
		test_runs, []() {}, WORKLOAD_TEMPLATE<pot::executors::thread_pool_executor_gs_hot>, PARAM, exec_gs_hot);       \
	auto t_ls_hot = pot::utils::time_it<std::chrono::duration<double>>(                                                \
		test_runs, []() {}, WORKLOAD_TEMPLATE<pot::executors::thread_pool_executor_ls_hot>, PARAM, exec_ls_hot);       \
	auto t_gpq = pot::utils::time_it<std::chrono::duration<double>>(                                                   \
		test_runs, []() {}, WORKLOAD_TEMPLATE<pot::executors::thread_pool_executor_gpq>, PARAM, exec_gpq);             \
	fmt::print("{:8} {:10} | {:8.5f} {:8.5f} {:8.5f} {:8.5f} {:8.5f} {:8.5f} {:8.5f} {:8.5f} {:8.5f}\n", thread_count, \
			   PARAM, t_gq.count(), t_lq.count(), t_lq_seq.count(), t_lq_neigh.count(), t_gs.count(), t_ls.count(),    \
			   t_gs_hot.count(), t_ls_hot.count(), t_gpq.count());

TEST_CASE("Comprehensive ExecutorType Thread Pool Benchmarks", "[benchmark]")
{
	const std::vector<int64_t> thread_counts = {std::thread::hardware_concurrency()};

	SECTION("1. Micro-Tasks")
	{
		const std::vector<int64_t> task_counts = {1'000'000, 3'000'000};
		const size_t test_runs = 5;

		fmt::print("\n=== S1: Micro-Tasks Contention ===\n");
		fmt::print("{:>8} {:>10} | {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8}\n", "Threads", "Tasks", "GQ",
				   "LQ", "LQ_Seq", "LQ_Neigh", "GS", "LS", "GS_Hot", "LS_Hot", "GPQ");
		fmt::print("{:-<110}\n", "");

		for (auto thread_count : thread_counts)
		{
			INIT_EXECUTORS(thread_count)
			for (auto param : task_counts)
			{
				RUN_BENCHMARK_SUITE(run_high_contention_microtasks, param)
			}
		}
	}

	SECTION("2. Mandelbrot Fractal")
	{
		const std::vector<int64_t> max_iters = {10};
		const size_t test_runs = 5;

		fmt::print("\n=== S3: Mandelbrot Imbalance ===\n");
		fmt::print("{:>8} {:>10} | {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8}\n", "Threads", "MaxIter", "GQ",
				   "LQ", "LQ_Seq", "LQ_Neigh", "GS", "LS", "GS_Hot", "LS_Hot", "GPQ");
		fmt::print("{:-<110}\n", "");

		for (auto thread_count : thread_counts)
		{
			INIT_EXECUTORS(thread_count)
			for (auto param : max_iters)
			{
				RUN_BENCHMARK_SUITE(build_mandelbrot_pot, param)
			}
		}
	}

	SECTION("3. Build A matrix")
	{
		const std::vector<int64_t> sizes = {20'000, 40'000};
		const size_t test_runs = 5;

		fmt::print("\n=== S4: Matrix Assembly ===\n");
		fmt::print("{:>8} {:>10} | {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8} {:>8}\n", "Threads", "Size", "GQ",
				   "LQ", "LQ_Seq", "LQ_Neigh", "GS", "LS", "GS_Hot", "LS_Hot", "GPQ");
		fmt::print("{:-<110}\n", "");

		for (auto thread_count : thread_counts)
		{
			INIT_EXECUTORS(thread_count)
			for (auto param : sizes)
			{
				RUN_BENCHMARK_SUITE(buildA_pot, param)
			}
		}
	}
}
