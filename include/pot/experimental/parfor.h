#pragma once

#include <cinttypes>
#include <utility>

#include "pot/when_all.h"
#include "pot/experimental/thread_pool/thread_pool_fpe.h"

namespace pot::experimental
{
    /**
    * \brief Parallel `for` taking into account chunks
    * \tparam static_chunk_size The size of the block that will be processed in one thread.
    *If negative, will be selected automatically
    * \tparam FuncType Type of function that handles iteration
    * \param executor pool of executors
    * \param from starting index
    * \param to ending index
    * \param func iteration processing function
    */
    template<int64_t static_chunk_size = -1, typename IndexType, typename FuncType = void(IndexType), bool mode = true>
    void parfor(thread_pool::thread_pool_fpe<mode>& executor, IndexType from, IndexType to, FuncType&& func)
        requires std::invocable<FuncType, IndexType>
    {
        assert(from < to);

        if (from >= to)
            return;

        const int64_t numIterations = to - from + 1;
        int64_t chunk_size = static_chunk_size;

        if (chunk_size < 0)
            chunk_size = std::max(1ull, numIterations / executor.thread_count());

        const int64_t numChunks = (numIterations + chunk_size - 1) / chunk_size;


        std::vector<std::future<void>> results;
        results.reserve(numChunks);

        for (int64_t chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex) {
            const IndexType chunkStart = from + IndexType(chunkIndex * chunk_size);
            const IndexType chunkEnd = std::min(IndexType(chunkStart + chunk_size), to);

            results.emplace_back(executor.run([=, &func] {
                for (IndexType i = chunkStart; i < chunkEnd; ++i) {
                    func(i);
                }
                }));
        }

        pot::when_all(results);
    }

}