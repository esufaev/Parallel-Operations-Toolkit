#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <omp.h>
#include <string>
#include <vector>

#include <Eigen/Dense>

#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/ranges.h>

#include "pot/algorithms/parfor.h"
#include "pot/executors/thread_pool_executor.h"
#include "pot/utils/time_it.h"

#include <concurrencpp/concurrencpp.h>

class fmt_logger
{
    std::ofstream m_file;

  public:
    fmt_logger(const std::string &filename)
    {
        m_file.open(filename, std::ios::out | std::ios::trunc);
        if (!m_file.is_open())
        {
            fmt::print(stderr, "Failed to open log file: {}\n", filename);
        }
    }

    void log_raw(const std::string &text)
    {
        fmt::print("{}", text);
        if (m_file.is_open())
            m_file << text;
    }

    void log_row(const std::string &row)
    {
        fmt::print("{}\n", row);
        if (m_file.is_open())
            m_file << row << "\n";
    }
};

std::pair<Eigen::MatrixXd, Eigen::VectorXi>
lu_decompose_concurrencpp(Eigen::MatrixXd A, std::shared_ptr<concurrencpp::executor> executor,
                          const std::pair<Eigen::Index, Eigen::Index> &boards_x =
                              {0, std::numeric_limits<Eigen::Index>::max()},
                          const std::pair<Eigen::Index, Eigen::Index> &boards_y = {
                              0, std::numeric_limits<Eigen::Index>::max()})
{
    Eigen::Index n = A.rows();
    Eigen::VectorXi p = Eigen::VectorXi::LinSpaced(n, 0, n - 1);

    for (Eigen::Index j = 0; j < n; ++j)
    {
        bool inboards = boards_y.first <= j && j <= boards_y.second;
        Eigen::Index I;

        A.col(j)
            .segment(std::max(j, boards_x.first), std::min(n - j, boards_x.second))
            .cwiseAbs()
            .maxCoeff(&I);

        I *= inboards;
        int row = I + j;

        A.row(j).swap(A.row(row));
        std::swap(p[j], p[row]);

        const Eigen::Index k_begin = j + 1;
        const Eigen::Index k_count = n - k_begin;

        for (Eigen::Index i = j + 1; i < n; ++i)
        {
            A(i, j) /= A(j, j);
        }

        if (k_count <= 0) [[unlikely]]
            continue;

        std::vector<concurrencpp::result<void>> results;
        const size_t actual_tasks = std::min<std::size_t>(std::thread::hardware_concurrency(),
                                                          static_cast<std::size_t>(k_count));
        results.reserve(actual_tasks);

        for (size_t t = 0; t < actual_tasks; ++t)
        {
            const Eigen::Index k_start =
                k_begin + static_cast<Eigen::Index>((t * k_count) / actual_tasks);
            const Eigen::Index k_end =
                k_begin + static_cast<Eigen::Index>(((t + 1) * k_count) / actual_tasks);

            results.push_back(executor->submit(
                [&A, j, n, k_start, k_end]()
                {
                    for (Eigen::Index k = k_start; k < k_end; ++k)
                    {
                        const double Ajk = A(j, k);
                        for (Eigen::Index i = j + 1; i < n; ++i)
                        {
                            A(i, k) -= A(i, j) * Ajk;
                        }
                    }
                }));
        }

        for (auto &res : results)
            res.get();
    }
    return {A, p};
}

std::pair<Eigen::MatrixXd, Eigen::VectorXi>
lu_decompose_linear(Eigen::MatrixXd A,
                    const std::pair<Eigen::Index, Eigen::Index> &boards_x =
                        {0, std::numeric_limits<Eigen::Index>::max()},
                    const std::pair<Eigen::Index, Eigen::Index> &boards_y = {
                        0, std::numeric_limits<Eigen::Index>::max()})
{
    Eigen::Index n = A.rows();
    Eigen::VectorXi p = Eigen::VectorXi::LinSpaced(n, 0, n - 1);

    for (Eigen::Index j = 0; j < n; ++j)
    {
        bool inboards = boards_y.first <= j && j <= boards_y.second;
        Eigen::Index I;
        Eigen::Index seg_len = 0;
        if (n > j)
        {
            Eigen::Index limit = (boards_x.second == std::numeric_limits<Eigen::Index>::max())
                                     ? (n - j)
                                     : std::min(n - j, boards_x.second);
            seg_len = limit;
        }

        if (seg_len > 0)
            A.col(j).segment(std::max(j, boards_x.first), seg_len).cwiseAbs().maxCoeff(&I);
        else
            I = 0;

        I *= inboards;
        int row = I + j;

        A.row(j).swap(A.row(row));
        std::swap(p[j], p[row]);

        for (Eigen::Index i = j + 1; i < n; ++i)
        {
            A(i, j) /= A(j, j);
            for (Eigen::Index k = j + 1; k < n; ++k)
            {
                A(i, k) -= A(i, j) * A(j, k);
            }
        }
    }
    return {A, p};
}

std::pair<Eigen::MatrixXd, Eigen::VectorXi>
lu_decompose_omp(Eigen::MatrixXd A,
                 const std::pair<Eigen::Index, Eigen::Index> &boards_x =
                     {0, std::numeric_limits<Eigen::Index>::max()},
                 const std::pair<Eigen::Index, Eigen::Index> &boards_y = {
                     0, std::numeric_limits<Eigen::Index>::max()})
{
    Eigen::Index n = A.rows();
    Eigen::VectorXi p = Eigen::VectorXi::LinSpaced(n, 0, n - 1);

    for (Eigen::Index j = 0; j < n; ++j)
    {
        bool inboards = boards_y.first <= j && j <= boards_y.second;
        Eigen::Index I;
        Eigen::Index seg_len = 0;
        if (n > j)
        {
            Eigen::Index limit = (boards_x.second == std::numeric_limits<Eigen::Index>::max())
                                     ? (n - j)
                                     : std::min(n - j, boards_x.second);
            seg_len = limit;
        }

        if (seg_len > 0)
            A.col(j).segment(std::max(j, boards_x.first), seg_len).cwiseAbs().maxCoeff(&I);
        else
            I = 0;

        I *= inboards;
        int row = I + j;

        A.row(j).swap(A.row(row));
        std::swap(p[j], p[row]);

        for (Eigen::Index i = j + 1; i < n; ++i)
        {
            A(i, j) /= A(j, j);
        }

        if (j + 1 < n)
        {
#pragma omp parallel for
            for (Eigen::Index k = j + 1; k < n; ++k)
            {
                const double Ajk = A(j, k);
                for (Eigen::Index i = j + 1; i < n; ++i)
                {
                    A(i, k) -= A(i, j) * Ajk;
                }
            }
        }
    }
    return {A, p};
}

std::pair<Eigen::MatrixXd, Eigen::VectorXi>
lu_decompose_parallel(Eigen::MatrixXd A, pot::executor &executor,
                      const std::pair<Eigen::Index, Eigen::Index> &boards_x =
                          {0, std::numeric_limits<Eigen::Index>::max()},
                      const std::pair<Eigen::Index, Eigen::Index> &boards_y = {
                          0, std::numeric_limits<Eigen::Index>::max()})
{
    Eigen::Index n = A.rows();
    Eigen::VectorXi p = Eigen::VectorXi::LinSpaced(n, 0, n - 1);

    for (Eigen::Index j = 0; j < n; ++j)
    {
        bool inboards = boards_y.first <= j && j <= boards_y.second;
        Eigen::Index I;
        Eigen::Index seg_len = 0;
        if (n > j)
        {
            Eigen::Index limit = (boards_x.second == std::numeric_limits<Eigen::Index>::max())
                                     ? (n - j)
                                     : std::min(n - j, boards_x.second);
            seg_len = limit;
        }

        if (seg_len > 0)
            A.col(j).segment(std::max(j, boards_x.first), seg_len).cwiseAbs().maxCoeff(&I);
        else
            I = 0;

        I *= inboards;
        int row = I + j;

        A.row(j).swap(A.row(row));
        std::swap(p[j], p[row]);

        for (Eigen::Index i = j + 1; i < n; ++i)
        {
            A(i, j) /= A(j, j);
        }

        if (j + 1 < n)
        {
            pot::algorithms::parfor(executor, j + 1, n,
                                    [&A, j, n](Eigen::Index k)
                                    {
                                        const double Ajk = A(j, k);
                                        for (Eigen::Index i = j + 1; i < n; ++i)
                                        {
                                            A(i, k) -= A(i, j) * Ajk;
                                        }
                                    })
                .get();
        }
    }
    return {A, p};
}

TEST_CASE("LU Decomposition")
{
    SECTION("Correctness on 5x5 Matrix")
    {
        Eigen::MatrixXd A(5, 5);
        A << 10.0, -7.0, 0.0, 5.0, -2.0, -3.0, 2.0, 6.0, 2.0, 1.0, 5.0, -1.0, 5.0, -2.0, 3.0, 4.0,
            3.0, -1.0, 7.0, 10.0, 2.0, -5.0, 2.0, 2.0, 1.0;

        auto [LU_ref, p_ref] = lu_decompose_linear(A);

        auto check_executor = [&](pot::executor &exec)
        {
            Eigen::MatrixXd A_copy = A;
            auto [LU_par, p_par] = lu_decompose_parallel(A_copy, exec);
            REQUIRE(p_ref == p_par);
            REQUIRE((LU_ref - LU_par).norm() < 1e-12);
        };

        pot::executors::thread_pool_executor_gq gq("test_gq", 4);
        pot::executors::thread_pool_executor_lq lq("test_lq", 4);
        pot::executors::thread_pool_executor_lfgq lfgq("test_lfgq", 4);
        pot::executors::thread_pool_executor_lflq lflq("test_lflq", 4);

        check_executor(gq);
        check_executor(lq);
        check_executor(lfgq);
        check_executor(lflq);

        {
            Eigen::MatrixXd A_omp = A;
            auto [LU_omp, p_omp] = lu_decompose_omp(A_omp);
            REQUIRE(p_ref == p_omp);
            REQUIRE((LU_ref - LU_omp).norm() < 1e-12);
        }
    }

    SECTION("Scaling Benchmark")
    {
        fmt_logger logger("tests.log");

        const std::vector<size_t> thread_counts = {12};
        const std::vector<Eigen::Index> matrix_sizes = {3000, 4000, 5000, 6000, 7000, 8000};
        const size_t iterations = 5;
        const std::pair<Eigen::Index, Eigen::Index> bounds = {
            0, std::numeric_limits<Eigen::Index>::max()};

        std::string h_sep = fmt::format("{:=^131}\n", " Scaling Benchmark Results ");
        std::string header_fmt =
            "| {:<4} | {:<5} | {:>13} | {:>13} | {:>13} | {:>13} | {:>13} | {:>13} |\n";
        std::string header_txt = fmt::format(fmt::runtime(header_fmt), "Th", "Size", "GQ(Mut)",
                                             "LQ(Mut)", "GQ(LF)", "LQ(LF)", "Cpp", "OMP");

        std::string row_sep =
            fmt::format("|{:-<6}|{:-<7}|{:-<15}|{:-<15}|{:-<15}|{:-<15}|{:-<15}|{:-<15}|\n", "", "",
                        "", "", "", "", "", "");

        logger.log_raw(h_sep);
        logger.log_raw(header_txt);
        logger.log_raw(row_sep);

        for (size_t num_threads : thread_counts)
        {
            omp_set_num_threads(static_cast<int>(num_threads));

            pot::executors::thread_pool_executor_gq exec_gq("bench_gq", num_threads);
            pot::executors::thread_pool_executor_lq exec_lq("bench_lq", num_threads);
            pot::executors::thread_pool_executor_lfgq exec_lfgq("bench_lfgq", num_threads);
            pot::executors::thread_pool_executor_lflq exec_lflq("bench_lflq", num_threads);

            concurrencpp::runtime concurrencpp_runtime;
            auto exec_cpp = concurrencpp_runtime.thread_pool_executor();

            for (Eigen::Index n : matrix_sizes)
            {
                Eigen::MatrixXd A_source = Eigen::MatrixXd::Random(n, n);

                auto t_gq = pot::utils::time_it<std::chrono::microseconds>(
                                iterations, []() {}, lu_decompose_parallel, A_source,
                                std::ref(exec_gq), bounds, bounds)
                                .count();

                auto t_lq = pot::utils::time_it<std::chrono::microseconds>(
                                iterations, []() {}, lu_decompose_parallel, A_source,
                                std::ref(exec_lq), bounds, bounds)
                                .count();

                auto t_lfgq = pot::utils::time_it<std::chrono::microseconds>(
                                  iterations, []() {}, lu_decompose_parallel, A_source,
                                  std::ref(exec_lfgq), bounds, bounds)
                                  .count();

                auto t_lflq = pot::utils::time_it<std::chrono::microseconds>(
                                  iterations, []() {}, lu_decompose_parallel, A_source,
                                  std::ref(exec_lflq), bounds, bounds)
                                  .count();

                auto t_cpp = pot::utils::time_it<std::chrono::microseconds>(
                                 iterations, []() {}, lu_decompose_concurrencpp, A_source, exec_cpp,
                                 bounds, bounds)
                                 .count();

                auto t_omp = pot::utils::time_it<std::chrono::microseconds>(
                                 iterations, []() {}, lu_decompose_omp, A_source, bounds, bounds)
                                 .count();

                std::string row_fmt =
                    "| {:<4} | {:<5} | {:>13} | {:>13} | {:>13} | {:>13} | {:>13} | {:>13} |";

                std::string line = fmt::format(fmt::runtime(row_fmt), num_threads, n, t_gq, t_lq,
                                               t_lfgq, t_lflq, t_cpp, t_omp);

                logger.log_row(line);
            }

            if (num_threads != thread_counts.back())
            {
                logger.log_raw(row_sep);
            }
        }
        logger.log_raw(fmt::format("{:=^131}\n\n", ""));
    }
}
