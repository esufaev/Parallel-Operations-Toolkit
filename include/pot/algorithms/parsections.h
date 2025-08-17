#pragma once

#include <vector>
#include <functional>
#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

#include "pot/executors/executor.h"

namespace pot::algorithms
{
    template<typename... Funcs> requires (std::is_invocable_v<Funcs> && ...)
    pot::coroutines::lazy_task<void> parsections(pot::executor& executor, Funcs&&... funcs)
    {
        static_assert(sizeof...(Funcs) > 0, "At least one function must be provided");

        std::vector<pot::coroutines::task<void>> tasks;

        (tasks.emplace_back(
            executor.run([f = std::forward<Funcs>(funcs)]() mutable -> pot::coroutines::task<void>
            {
                using Ret = std::invoke_result_t<decltype(f)&>;

                if constexpr (pot::traits::is_task_v<Ret> || pot::traits::is_lazy_task_v<Ret>)
                {
                    co_await std::invoke(f);
                }
                else
                {
                    std::invoke(f);
                }
             
                co_return;
            })
        ), ...);

        for (auto it = tasks.begin(); it != tasks.end(); ++it)
            std::move(*it).sync_wait();

        co_return;
    }
}
