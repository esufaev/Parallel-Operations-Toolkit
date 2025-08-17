#pragma once

#include "pot/algorithms/reduce.h"

namespace pot::algorithms
{
    /**
     * @brief Asynchronously computes the dot product of two arrays using SIMD.
     *
     * @tparam T  Element type (must be arithmetic).
     * @tparam ST SIMD type (pot::simd::SIMDType).
     * @param exec Executor for task scheduling.
     * @param a    First input span.
     * @param b    Second input span.
     * @return lazy_task<T> The computed dot product.
     *
     * @throws std::invalid_argument if the spans have different sizes.
     */
    template <typename T, pot::simd::SIMDType ST>
    [[nodiscard]] pot::coroutines::lazy_task<T>
    dot_simd(pot::executor &exec, std::span<const T> a, std::span<const T> b)
        requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("dot_simd: spans must have equal sizes");
        return dot_simd<T, ST>(exec, a.data(), b.data(), a.size());
    }

    /**
     * @brief Asynchronously computes the SIMD dot product of two vectors.
     * @copydetails dot_simd(pot::executor&, std::span<const T>, std::span<const T>)
     */
    template <typename T, pot::simd::SIMDType ST>
    [[nodiscard]] pot::coroutines::lazy_task<T>
    dot_simd(pot::executor &exec, const std::vector<T> &a, const std::vector<T> &b)
        requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("dot_simd: vectors must have equal sizes");
        return dot_simd<T, ST>(exec, std::span<const T>(a), std::span<const T>(b));
    }

    /**
     * @brief Asynchronously computes the dot product of two arrays without SIMD.
     *
     * @tparam T  Element type (must be arithmetic).
     * @param exec Executor for task scheduling.
     * @param a    First input span.
     * @param b    Second input span.
     * @return lazy_task<T> The computed dot product.
     *
     * @throws std::invalid_argument if the spans have different sizes.
     */
    template <typename T>
    [[nodiscard]] pot::coroutines::lazy_task<T>
    dot(pot::executor &exec, std::span<const T> a, std::span<const T> b)
        requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("dot: spans must have equal sizes");
        return dot<T>(exec, a.data(), b.data(), a.size());
    }

    /**
     * @brief Asynchronously computes the dot product of two vectors.
     * @copydetails dot(pot::executor&, std::span<const T>, std::span<const T>)
     */
    template <typename T>
    [[nodiscard]] pot::coroutines::lazy_task<T>
    dot(pot::executor &exec, const std::vector<T> &a, const std::vector<T> &b)
        requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("dot: vectors must have equal sizes");
        return dot<T>(exec, std::span<const T>(a), std::span<const T>(b));
    }

} // namespace pot::algorithms
