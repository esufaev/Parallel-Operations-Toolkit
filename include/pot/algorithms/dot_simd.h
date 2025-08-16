#pragma once

#include <cstddef>
#include <vector>
#include <span>
#include <algorithm>
#include <type_traits>

#include "pot/simd/simd_forced.h"
#include "pot/algorithms/parfor.h"

namespace pot::algorithms
{
    template <typename T, pot::simd::SIMDType ST> [[nodiscard]] pot::coroutines::lazy_task<T> 
    dot_simd(pot::executor &exec, const T *a, const T *b, std::size_t n) requires(std::is_arithmetic_v<T>)
    {
        constexpr size_t lane_width = pot::simd::details::simd_traits<T, ST>::scalar_count;

        if (n == 0) co_return static_cast<T>(0);

        const size_t block_count = std::min<size_t>(std::max(size_t(1), exec.thread_count()), std::max<size_t>(size_t(1), n / lane_width));
        const size_t elems_per_block = std::max(lane_width, (n + block_count - 1) / block_count);

        std::vector<T> partial_sums(block_count, static_cast<T>(0));

        co_await pot::algorithms::parfor(exec, static_cast<size_t>(0), block_count, [=, &partial_sums](size_t block_idx)
        {
            const size_t block_begin = block_idx * elems_per_block;
            const size_t block_end   = std::min(n, block_begin + elems_per_block);

            pot::simd::simd_forced<T, ST> acc = pot::simd::simd_forced<T, ST>::zeros();

            size_t i = block_begin;
            for (; i + lane_width <= block_end; i += lane_width)
            {
                pot::simd::simd_forced<T, ST> va; va.loadu(a + i);
                pot::simd::simd_forced<T, ST> vb; vb.loadu(b + i);
                acc += (va * vb);
            }

            alignas((alignof(T) > alignof(std::max_align_t) ?
                    alignof(T) :
                    alignof(std::max_align_t))) T lane_buf[lane_width];

            acc.storeu(lane_buf);

            T block_sum = 0;
            for (size_t k = 0; k < lane_width; ++k) block_sum += lane_buf[k];
            for (; i < block_end; ++i) block_sum += a[i] * b[i];

            partial_sums[block_idx] = block_sum;
        });

        T total = 0;
        for (T v : partial_sums) total += v;

        co_return total;
    }

    template <typename T, pot::simd::SIMDType ST> [[nodiscard]] pot::coroutines::lazy_task<T>
    dot_simd(pot::executor &exec, std::span<const T> a, std::span<const T> b) requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size()) throw std::invalid_argument("dot_simd: spans must have equal sizes");
        return dot_simd<T, ST>(exec, a.data(), b.data(), a.size());
    }

    template <typename T, pot::simd::SIMDType ST> [[nodiscard]] pot::coroutines::lazy_task<T>
    dot_simd(pot::executor &exec, const std::vector<T> &a, std::span<const T> b) requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size()) throw std::invalid_argument("dot_simd: span and vecotr must have equal sizes");
        return dot_simd<T, ST>(exec, a.data(), b.data(), a.size());
    }

    template <typename T, pot::simd::SIMDType ST> [[nodiscard]] pot::coroutines::lazy_task<T>
    dot_simd(pot::executor &exec, std::span<const T> a, const std::vector<T> &b) requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size()) throw std::invalid_argument("dot_simd: span and vecotr must have equal sizes");
        return dot_simd<T, ST>(exec, a.data(), b.data(), a.size());
    }

    template <typename T, pot::simd::SIMDType ST> [[nodiscard]] pot::coroutines::lazy_task<T>
    dot_simd(pot::executor &exec, const std::vector<T> &a, const std::vector<T> &b) requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size()) throw std::invalid_argument("dot_simd: vectors must have equal sizes");
        return dot_simd<T, ST>(exec, std::span<const T>(a), std::span<const T>(b));
    }

    template <typename T, pot::simd::SIMDType ST> [[nodiscard]] pot::coroutines::lazy_task<T>
    dot_simd(pot::executor &exec, const T *a_begin, const T *a_end, const T *b_begin) requires(std::is_arithmetic_v<T>)
    {
        return dot_simd<T, ST>(exec, a_begin, b_begin, static_cast<size_t>(a_end - a_begin));
    }

} // namespace pot::algorithms
