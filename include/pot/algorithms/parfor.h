#pragma once

#include <vector>
#include <functional>
#include <cassert>
#include <memory>

#include "pot/coroutines/when_all.h"
#include "pot/executors/executor.h"

namespace pot::algorithms
{
    template <int64_t static_chunk_size = -1, typename IndexType, typename FuncType = void(IndexType)>
        requires std::invocable<FuncType, IndexType>
    pot::coroutines::lazy_task<void> parfor(pot::executor &executor, IndexType from, IndexType to, FuncType func)
    {
        assert(from < to);

        const int64_t numIterations = to - from;
        int64_t chunk_size = static_chunk_size;

        if (chunk_size < 0)
            chunk_size = std::max<int64_t>(1, numIterations / executor.thread_count());

        const int64_t numChunks = (numIterations + chunk_size - 1) / chunk_size;
        std::vector<std::shared_ptr<pot::coroutines::task<void>>> tasks;
        tasks.reserve(numChunks);

        for (int64_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
        {
            const IndexType chunkStart = from + IndexType(chunkIndex * chunk_size);
            const IndexType chunkEnd = std::min<IndexType>(chunkStart + IndexType(chunk_size), to);

            auto task = std::make_shared<pot::coroutines::task<void>>([chunkStart, chunkEnd, func]() -> pot::coroutines::task<void>
                                                                      {
                for (IndexType i = chunkStart; i < chunkEnd; ++i)
                {
                    if constexpr (std::is_invocable_r_v<pot::coroutines::task<void>, FuncType, IndexType>) {
                        co_await std::invoke(func, i);
                    } else {
                        std::invoke(func, i);
                    }
                }
                co_return; }());

            tasks.push_back(task);
        }

        co_await pot::coroutines::when_all(tasks.begin(), tasks.end());
        co_return;
    }
}
