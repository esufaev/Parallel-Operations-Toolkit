#pragma once

#include <vector>
#include <functional>
#include <cassert>
#include <memory>

#include "pot/executors/executor.h"

namespace pot::algorithms
{
    template <int64_t static_chunk_size = -1, typename IndexType, typename FuncType = void(IndexType)>
        requires std::invocable<FuncType &, IndexType>
    pot::coroutines::lazy_task<void>
    parfor(pot::executor &executor, IndexType from, IndexType to, FuncType&& func)
    {
        assert(from < to);

        const int64_t numIterations = static_cast<int64_t>(to - from);

        int64_t chunk_size = static_chunk_size;
        if (chunk_size < 0)
            chunk_size = std::max<int64_t>(1, numIterations / static_cast<int64_t>(executor.thread_count()));

        const int64_t numChunks = (numIterations + chunk_size - 1) / chunk_size;

        std::vector<pot::coroutines::task<void>> tasks;
        tasks.reserve(static_cast<size_t>(numChunks));

        for (int64_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
        {
            const IndexType chunkStart = from + static_cast<IndexType>(chunkIndex * chunk_size);
            const IndexType chunkEnd = std::min<IndexType>(chunkStart + static_cast<IndexType>(chunk_size), to);

            auto task = executor.run(
                [chunkStart, chunkEnd, func = std::forward<FuncType>(func)]() mutable -> pot::coroutines::task<void>
                {
                    for (IndexType i = chunkStart; i < chunkEnd; ++i)
                    {
                        using Ret = std::invoke_result_t<decltype(func) &, IndexType>;
                        if constexpr (pot::traits::is_task_v<Ret> || pot::traits::is_lazy_task_v<Ret>)
                        {
                            co_await std::invoke(func, i);
                        }
                        else
                        {
                            std::invoke(func, i);
                        }
                    }
                    co_return;
                });

            tasks.emplace_back(std::move(task));
        }

        for (auto it = tasks.begin(); it != tasks.end(); ++it)
            std::move(*it).sync_wait();
        co_return;
    }
}
