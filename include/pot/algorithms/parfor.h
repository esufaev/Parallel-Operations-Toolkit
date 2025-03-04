#pragma once

#include <cinttypes>
#include <utility>
#include <vector>
#include <coroutine>
#include <memory>
#include <cassert>

#include "pot/when_all.h"
#include "pot/executors/thread_pool_executor.h"

namespace pot::algorithms
{
    template <int64_t static_chunk_size = -1, typename IndexType, typename FuncType = void(IndexType)>
        requires std::invocable<FuncType, IndexType>
    pot::coroutines::task<void> parfor(pot::executor &executor, IndexType from, IndexType to, FuncType &&func)
    {
        assert(from < to);

        const int64_t numIterations = to - from + 1;
        int64_t chunk_size = static_chunk_size;

        if (chunk_size < 0)
            chunk_size = std::max<int64_t>(1, numIterations / executor.thread_count());

        const int64_t numChunks = (numIterations + chunk_size - 1) / chunk_size;
        std::vector<pot::coroutines::task<void>> tasks;
        tasks.reserve(numChunks);

        for (int64_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
        {
            const IndexType chunkStart = from + IndexType(chunkIndex * chunk_size);
            const IndexType chunkEnd = std::min<IndexType>(chunkStart + IndexType(chunk_size), to);

            tasks.push_back(executor.run([chunkStart, chunkEnd, &func]() -> pot::coroutines::task<void>
                                         {
                for (IndexType i = chunkStart; i < chunkEnd; ++i)
                {
                    if constexpr (std::is_invocable_r_v<pot::coroutines::task<void>, FuncType, IndexType>)
                    {
                        co_await func(i);
                    }
                    else 
                    {
                        func(i);
                    }
                    co_return;
                } }));
        }

        co_return co_await pot::when_all(tasks.begin(), tasks.end());
    }
}