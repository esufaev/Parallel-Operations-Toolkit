#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "pot/algorithms/parfor.h"
#include "pot/executors/thread_pool_executor.h"
#include "pot/sync/async_lock.h"
#include "pot/utils/time_it.h"

#include <Eigen/Sparse>
#include <concurrencpp/concurrencpp.h>
#include <fmt/core.h>
#include <omp.h>

template <typename ResultType> using async = concurrencpp::result<ResultType>;
using async_void = async<void>;

template <typename T> struct is_coroutine_return_type : std::false_type
{
};
template <typename T> struct is_coroutine_return_type<async<T>> : std::true_type
{
};

template <typename T> struct coroutine_return_type;
template <typename T> struct coroutine_return_type<async<T>>
{
    using type = T;
};

template <typename T> using coroutine_return_type_t = typename coroutine_return_type<T>::type;

template <typename T> struct remove_std_function
{
    using type = T;
};
template <typename T> struct remove_std_function<std::function<T>>
{
    using type = T;
};
template <typename T> using remove_std_function_t = typename remove_std_function<T>::type;

template <typename Function, typename... Args>
concept Coroutine =
    (std::is_function_v<remove_std_function_t<Function>> || std::invocable<Function, Args...>) &&
    is_coroutine_return_type<std::invoke_result_t<Function, Args...>>::value;

template <typename Function, typename... Args>
    requires Coroutine<Function, Args...>
struct CoroutineTypeInfo
{
    using return_type = std::invoke_result_t<Function, Args...>;
    using return_type_value = coroutine_return_type_t<return_type>;
};

template <typename ValueType>
int64_t as_int64_t(const ValueType &value)
    requires requires { value.value_; }
{
    return static_cast<int64_t>(value.value_);
}
inline int64_t as_int64_t(const int64_t value) { return value; }

template <typename ExecutorType, typename... InitializationArgs>
    requires std::is_base_of_v<concurrencpp::executor, ExecutorType>
auto make_executor_ptr(InitializationArgs &&...args)
{
    return std::shared_ptr<ExecutorType>(
        new ExecutorType(std::forward<InitializationArgs>(args)...),
        [](ExecutorType *e)
        {
            if (e)
            {
                try
                {
                    e->shutdown();
                }
                catch (...)
                {
                    std::abort();
                }
                delete e;
            }
        });
}

template <typename Func, typename... FuncArgs>
    requires Coroutine<Func, FuncArgs...>
auto run_parallel_coroutine(concurrencpp::executor_tag,
                            std::shared_ptr<concurrencpp::executor> executor, Func func,
                            FuncArgs... func_args) ->
    typename CoroutineTypeInfo<Func, FuncArgs...>::return_type
{
    auto args_copy = std::make_tuple(func_args...);
    co_return co_await std::apply(func, args_copy);
}

template <typename Func, typename... FuncArgs>
    requires Coroutine<Func, FuncArgs...>
auto run_co_async(std::shared_ptr<concurrencpp::executor> executor, Func func,
                  FuncArgs... func_args) ->
    typename CoroutineTypeInfo<Func, FuncArgs...>::return_type
{
    return run_parallel_coroutine(concurrencpp::executor_tag{}, executor, func, func_args...);
}

template <typename Func, typename... FuncArgs>
    requires(std::invocable<Func, FuncArgs...> && (not Coroutine<Func, FuncArgs...>))
auto run_co_async(const std::shared_ptr<concurrencpp::executor> &executor, Func &&func,
                  FuncArgs &&...func_args) -> async<std::invoke_result_t<Func, FuncArgs...>>
{
    return executor->submit(std::forward<Func>(func), std::forward<FuncArgs>(func_args)...);
}

template <int64_t static_chunk_size = -1, typename IndexType, typename FuncType = void(IndexType)>
[[nodiscard]] async_void parfor_async(const std::shared_ptr<concurrencpp::executor> &executor,
                                      IndexType from, IndexType to, FuncType func)
{
    assert(from < to);
    if (from >= to)
        co_return;

    const IndexType numIterations = to - from + IndexType(1);
    IndexType chunk_size = IndexType(static_chunk_size);
    if (chunk_size < IndexType(0))
        chunk_size =
            std::max(IndexType(1), numIterations / IndexType(executor->max_concurrency_level()));

    const IndexType numChunks = (numIterations + chunk_size - IndexType(1)) / chunk_size;

    std::vector<async_void> results;
    results.reserve(as_int64_t(numChunks));

    for (auto chunkIndex = IndexType(0); chunkIndex < numChunks; ++chunkIndex)
    {
        const IndexType chunkStart = from + IndexType(chunkIndex * chunk_size);
        const IndexType chunkEnd = std::min(IndexType(chunkStart + chunk_size), to);
        results.emplace_back(run_co_async(executor,
                                          [=]() -> async_void
                                          {
                                              for (IndexType i = chunkStart; i < chunkEnd; ++i)
                                              {
                                                  if constexpr (Coroutine<FuncType, IndexType>)
                                                      co_await func(i);
                                                  else
                                                      func(i);
                                              }
                                              co_return;
                                          }));
    }
    co_await concurrencpp::when_all(executor, results.begin(), results.end());
    co_return;
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

void buildA_omp(const int64_t n)
{
    Eigen::SparseMatrix<double> A(n, n);
    A.resize(n, n);
#pragma omp parallel
    {
#pragma omp for
        for (int64_t i = 0; i < n; ++i)
        {
            auto [vl, vc, vr] = calculation(i);
            if (i > 0)
            {
#pragma omp critical
                A.coeffRef(i, i - 1) = vl;
            }
#pragma omp critical
            A.coeffRef(i, i) = vc;
            if (i < n - 1)
            {
#pragma omp critical
                A.coeffRef(i, i + 1) = vr;
            }
        }
    }
}

void buildA_coroutine(const int64_t n, std::shared_ptr<concurrencpp::thread_pool_executor> executor)
{
    Eigen::SparseMatrix<double> A(n, n);
    A.resize(n, n);
    concurrencpp::async_lock locker;
    parfor_async(executor, (long int)0, n,
                 [&](const int64_t i) -> async_void
                 {
                     auto [vl, vc, vr] = calculation(i);
                     auto _ = co_await locker.lock(executor);
                     if (i > 0)
                         A.coeffRef(i, i - 1) = vl;
                     A.coeffRef(i, i) = vc;
                     if (i < n - 1)
                         A.coeffRef(i, i + 1) = vr;
                     co_return;
                 })
        .get();
}

template <typename Executor> void buildA_pot(const int64_t n, std::shared_ptr<Executor> executor)
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
                                auto _ = co_await locker.lock();

                                if (i > 0)
                                    A.coeffRef(i, i - 1) = vl;
                                A.coeffRef(i, i) = vc;
                                if (i < n - 1)
                                    A.coeffRef(i, i + 1) = vr;
                            })
        .get();
}

TEST_CASE("Matrix Assembly Performance", "[benchmark]")
{
    const std::vector<int64_t> thread_counts = {12};
    const std::vector<int64_t> sizes = {40'000};
    const size_t test_count = 3;

    SECTION("Compare Executors")
    {
        fmt::print("{:>8} {:>8} | {:>10} {:>10} | {:>10} {:>10} {:>10} {:>10}\n", "Threads", "Size",
                   "OMP", "CppCoro", "GQ", "LQ", "LFGQ", "LFLQ");
        fmt::print("{:-<90}\n", "");

        for (auto thread_count : thread_counts)
        {
            omp_set_num_threads(static_cast<int>(thread_count));

            auto exec_ccpp = make_executor_ptr<concurrencpp::thread_pool_executor>(
                "ThreadPool", thread_count, std::chrono::seconds(60));

            auto exec_gq =
                std::make_shared<pot::executors::thread_pool_executor_gq>("GQ", thread_count);
            auto exec_lq =
                std::make_shared<pot::executors::thread_pool_executor_lq>("LQ", thread_count);
            auto exec_lfgq =
                std::make_shared<pot::executors::thread_pool_executor_lfgq>("LFGQ", thread_count);
            auto exec_lflq =
                std::make_shared<pot::executors::thread_pool_executor_lflq>("LFLQ", thread_count);

            for (auto n : sizes)
            {
                auto time_omp = pot::utils::time_it<std::chrono::duration<double>>(
                    test_count, []() {}, buildA_omp, n);

                auto time_ccpp = pot::utils::time_it<std::chrono::duration<double>>(
                    test_count, []() {}, buildA_coroutine, n, exec_ccpp);

                auto time_gq = pot::utils::time_it<std::chrono::duration<double>>(
                    test_count, []() {}, buildA_pot<pot::executors::thread_pool_executor_gq>, n,
                    exec_gq);

                auto time_lq = pot::utils::time_it<std::chrono::duration<double>>(
                    test_count, []() {}, buildA_pot<pot::executors::thread_pool_executor_lq>, n,
                    exec_lq);

                auto time_lfgq = pot::utils::time_it<std::chrono::duration<double>>(
                    test_count, []() {}, buildA_pot<pot::executors::thread_pool_executor_lfgq>, n,
                    exec_lfgq);

                auto time_lflq = pot::utils::time_it<std::chrono::duration<double>>(
                    test_count, []() {}, buildA_pot<pot::executors::thread_pool_executor_lflq>, n,
                    exec_lflq);

                fmt::print("{:8} {:8} | {:10.5f} {:10.5f} | {:10.5f} {:10.5f} {:10.5f} {:10.5f}\n",
                           thread_count, n, time_omp.count(), time_ccpp.count(), time_gq.count(),
                           time_lq.count(), time_lfgq.count(), time_lflq.count());
            }
        }
    }
}
