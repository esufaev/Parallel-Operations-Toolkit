#pragma once

#include "pot/algorithms/reduce.h"

namespace pot::algorithms
{
    template <typename T, pot::simd::SIMDType ST>
    [[nodiscard]] pot::coroutines::lazy_task<T>
    dot_simd(pot::executor &exec, std::span<const T> a, std::span<const T> b)
        requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("dot_simd: spans must have equal sizes");
        return dot_simd<T, ST>(exec, a.data(), b.data(), a.size());
    }

    template <typename T, pot::simd::SIMDType ST>
    [[nodiscard]] pot::coroutines::lazy_task<T>
    dot_simd(pot::executor &exec, const std::vector<T> &a, const std::vector<T> &b)
        requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("dot_simd: vectors must have equal sizes");
        return dot_simd<T, ST>(exec, std::span<const T>(a), std::span<const T>(b));
    }

    template <typename T>
    [[nodiscard]] pot::coroutines::lazy_task<T>
    dot(pot::executor &exec, std::span<const T> a, std::span<const T> b)
        requires(std::is_arithmetic_v<T>)
    {
        if (a.size() != b.size())
            throw std::invalid_argument("dot: spans must have equal sizes");
        return dot<T>(exec, a.data(), b.data(), a.size());
    }

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
