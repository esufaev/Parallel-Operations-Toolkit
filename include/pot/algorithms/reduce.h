#pragma once

#include <cstddef>
#include <vector>
#include <span>
#include <algorithm>
#include <type_traits>
#include <stdexcept>
#include <functional>

#include "pot/simd/simd_forced.h"
#include "pot/algorithms/parfor.h"

namespace pot::algorithms
{
    /**
     * @brief Asynchronously computes an element-wise reduction of two arrays.
     *
     * Applies @p elem_op to each pair of elements (a[i], b[i]) and then reduces
     * the results with @p reduce_op, starting from @p identity.
     *
     * @tparam T         Input element type (must be arithmetic).
     * @tparam R         Result type (must be arithmetic, defaults to T).
     * @tparam ElemOp    Callable type: (T, T) -> R.
     * @tparam ReduceOp  Callable type: (R, R) -> R.
     *
     * @param exec     Executor for task scheduling.
     * @param a        Pointer to first array.
     * @param b        Pointer to second array.
     * @param n        Number of elements.
     * @param elem_op  Binary operation applied element-wise.
     * @param reduce_op Reduction operation to combine results.
     * @param identity Identity element for the reduction.
     *
     * @return lazy_task<R> The reduced result.
     */
    template <typename T, typename R = T, typename ElemOp = std::plus<T>, typename ReduceOp = std::plus<R>>
    [[nodiscard]] pot::coroutines::lazy_task<R>
    elementwise_reduce(pot::executor &exec, const T *a, const T *b, std::size_t n, ElemOp elem_op, ReduceOp reduce_op, R identity)
        requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<R>)
    {
        if (n == 0) co_return identity;

        const std::size_t block_count = std::max<std::size_t>(1, exec.thread_count());
        const std::size_t block_size = (n + block_count - 1) / block_count;

        std::vector<R> partial(block_count, identity);

        co_await pot::algorithms::parfor(exec, static_cast<size_t>(0), block_count,
        [=, &partial](std::size_t block_idx)
        {
            const std::size_t begin = block_idx * block_size;
            const std::size_t end = std::min(n, begin + block_size);

            R sum = identity;
            for (std::size_t i = begin; i < end; ++i)
            {
                sum = reduce_op(sum, elem_op(a[i], b[i]));
            }
            partial[block_idx] = sum;
        });

        R total = identity;
        for (const R &v : partial)
            total = reduce_op(total, v);
        co_return total;
    }

    /**
     * @brief Convenience overload of elementwise_reduce for std::span.
     * @copydetails elementwise_reduce(pot::executor&, const T*, const T*, std::size_t, ElemOp, ReduceOp, R)
     * @throws std::invalid_argument if the spans differ in size.
     */
    template <typename T, typename R = T, typename ElemOp = std::plus<T>, typename ReduceOp = std::plus<T>>
    [[nodiscard]] pot::coroutines::lazy_task<R>
    elementwise_reduce(pot::executor &exec, std::span<const T> a, std::span<const T> b, ElemOp elem_op, ReduceOp reduce_op, R identity)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("elementwise_reduce: spans must have equal sizes");
        return elementwise_reduce<T, R>(exec, a.data(), b.data(), a.size(), elem_op, reduce_op, identity);
    }

    /**
     * @brief Convenience overload of elementwise_reduce for std::vector.
     * @copydetails elementwise_reduce(pot::executor&, const T*, const T*, std::size_t, ElemOp, ReduceOp, R)
     * @throws std::invalid_argument if the vectors differ in size.
     */
    template <typename T, typename R, typename ElemOp, typename ReduceOp>
    [[nodiscard]] pot::coroutines::lazy_task<R>
    elementwise_reduce(pot::executor &exec, const std::vector<T> &a, const std::vector<T> &b, ElemOp elem_op, ReduceOp reduce_op, R identity)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("elementwise_reduce: vectors must have equal sizes");
        return elementwise_reduce<T, R>(exec, std::span<const T>(a), std::span<const T>(b), elem_op, reduce_op, identity);
    }

    /**
     * @brief Asynchronously computes an element-wise reduction using SIMD acceleration.
     *
     * Loads and processes multiple elements per iteration using SIMD instructions.
     * Falls back to scalar processing for remaining elements.
     *
     * @tparam T             Input element type (must be arithmetic).
     * @tparam R             Result type (must be arithmetic).
     * @tparam ST            SIMD type (pot::simd::SIMDType).
     * @tparam SimdElemOp    Callable: (simd_forced<T>, simd_forced<T>) -> simd_forced<R>.
     * @tparam ScalarElemOp  Callable: (T, T) -> R.
     * @tparam ReduceOp      Callable: (R, R) -> R.
     *
     * @param exec          Executor for task scheduling.
     * @param a             Pointer to first array.
     * @param b             Pointer to second array.
     * @param n             Number of elements.
     * @param simd_elem_op  Operation applied to SIMD registers.
     * @param scalar_elem_op Operation applied to leftover scalar elements.
     * @param reduce_op     Reduction operator.
     * @param identity      Identity element for reduction.
     *
     * @return lazy_task<R> The reduced result.
     */
    template < typename T, typename R = T,
        pot::simd::SIMDType ST, 
        typename SimdElemOp, typename ScalarElemOp, typename ReduceOp>
    [[nodiscard]] pot::coroutines::lazy_task<R>
    elementwise_reduce_simd(pot::executor &exec, const T *a, const T *b, std::size_t n,
                            SimdElemOp simd_elem_op, ScalarElemOp scalar_elem_op, ReduceOp reduce_op, R identity)
    requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<R>)
    {
        using simd_t_i = pot::simd::simd_forced<T, ST>;
        using simd_t_res = pot::simd::simd_forced<R, ST>;
        constexpr std::size_t scalar_count = pot::simd::details::simd_traits<T, ST>::scalar_count;

        if (n == 0)
            co_return identity;

        const std::size_t block_count = std::min<std::size_t>(
            std::max<std::size_t>(1, exec.thread_count()),
            std::max<std::size_t>(std::size_t(1), n / scalar_count));
        const std::size_t elems_per_block = std::max<std::size_t>(scalar_count, (n + block_count - 1) / block_count);

        std::vector<R> partial(block_count, identity);

        co_await pot::algorithms::parfor(exec, static_cast<size_t>(0), block_count,
        [=, &partial](std::size_t block_idx)
        {
            const std::size_t begin = block_idx * elems_per_block;
            const std::size_t end = std::min(n, begin + elems_per_block);

            simd_t_res sum = simd_t_res::zeros();

            std::size_t i = begin;
            for (; i + scalar_count <= end; i += scalar_count)
            {
                simd_t_i va; va.loadu(a + i);
                simd_t_i vb; vb.loadu(b + i);
                sum += simd_elem_op(va, vb);
            }

            alignas((alignof(R) > alignof(std::max_align_t) ? alignof(R) : alignof(std::max_align_t))) R lane[scalar_count];

            sum.storeu(lane);

            R block_sum = identity;
            for (std::size_t k = 0; k < scalar_count; ++k) block_sum = reduce_op(block_sum, lane[k]);
            for (; i < end; ++i) block_sum = reduce_op(block_sum, scalar_elem_op(a[i], b[i]));

            partial[block_idx] = block_sum;
        });

        R total = identity;
        for (const R &v : partial) total = reduce_op(total, v);
        co_return total;
    }

    /**
     * @brief Convenience overload of elementwise_reduce_simd for std::span.
     * @copydetails elementwise_reduce_simd(pot::executor&, const T*, const T*, std::size_t, SimdElemOp, ScalarElemOp, ReduceOp, R)
     * @throws std::invalid_argument if the spans differ in size.
     */
    template <typename T, typename R, pot::simd::SIMDType ST, typename SimdElemOp, typename ScalarElemOp, typename ReduceOp>
    [[nodiscard]] pot::coroutines::lazy_task<R>
    elementwise_reduce_simd(pot::executor &exec, std::span<const T> a, std::span<const T> b,
                            SimdElemOp simd_elem_op, ScalarElemOp scalar_elem_op, ReduceOp reduce_op, R identity)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("elementwise_reduce_simd: spans must have equal sizes");
        return elementwise_reduce_simd<T, R, ST>(exec, a.data(), b.data(), a.size(),
                                                 simd_elem_op, scalar_elem_op, reduce_op, identity);
    }

    /**
     * @brief Convenience overload of elementwise_reduce_simd for std::vector.
     * @copydetails elementwise_reduce_simd(pot::executor&, const T*, const T*, std::size_t, SimdElemOp, ScalarElemOp, ReduceOp, R)
     * @throws std::invalid_argument if the vectors differ in size.
     */
    template <typename T, typename R, pot::simd::SIMDType ST, typename SimdElemOp, typename ScalarElemOp, typename ReduceOp>
    [[nodiscard]] pot::coroutines::lazy_task<R>
    elementwise_reduce_simd(pot::executor &exec, const std::vector<T> &a, const std::vector<T> &b,
                            SimdElemOp simd_elem_op, ScalarElemOp scalar_elem_op, ReduceOp reduce_op, R identity)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("elementwise_reduce_simd: vectors must have equal sizes");
        return elementwise_reduce_simd<T, R, ST>(exec, std::span<const T>(a), std::span<const T>(b),
                                                 simd_elem_op, scalar_elem_op, reduce_op, identity);
    }
} // namespace pot::algorithms
