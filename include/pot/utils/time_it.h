#pragma once

#include <chrono>
#include <functional>

namespace pot::utils
{
template <typename DurationType, typename IterationCleanUpCallback, typename Func, typename... Args>
DurationType time_it(size_t n, IterationCleanUpCallback &&callback, Func &&func, Args &&...args)
    requires std::invocable<Func, Args...> && requires {
        {
            std::chrono::duration_cast<DurationType>(std::chrono::high_resolution_clock::now() -
                                                     std::chrono::high_resolution_clock::now())
        };
    }
{
    using clock_type = std::chrono::high_resolution_clock;

    clock_type::duration duration{0};

    for (size_t i = 0; i < n; i++)
    {
        const auto start = clock_type::now();
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        const auto end = clock_type::now();
        duration += end - start;

        std::invoke(std::forward<IterationCleanUpCallback>(callback));
    }

    return std::chrono::duration_cast<DurationType>(duration / n);
}

template <typename DurationType, typename Func, typename... Args>
DurationType time_it(Func &&func, Args &&...args)
    requires std::invocable<Func, Args...> && requires {
        {
            std::chrono::duration_cast<DurationType>(std::chrono::high_resolution_clock::now() -
                                                     std::chrono::high_resolution_clock::now())
        };
    }
{
    return time_it<DurationType, Func, Args...>(1, {}, std::forward<Func>(func),
                                                std::forward<Args>(args)...);
}
} // namespace pot::utils
