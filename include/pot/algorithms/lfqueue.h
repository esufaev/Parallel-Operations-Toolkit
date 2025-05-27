#include <atomic>
#include <thread>
#include <vector>

#include "pot/utils/cache_line.h"

namespace pot::algorithms
{
    template <typename T>
    class alignas(pot::cache_line_alignment) lfqueue
    {
        struct alignas(pot::cache_line_alignment) cell_t
        {
            std::atomic<size_t> m_sequence;
            T                   m_data;
        };

        alignas(pot::cache_line_alignment) std::vector<cell_t> buffer;
        alignas(pot::cache_line_alignment) size_t              buffer_mask;
        alignas(pot::cache_line_alignment) std::atomic<size_t> epos, dpos;

    public:
        lfqueue(size_t size) : buffer(size), buffer_mask(size - 1)
        {
            if (size > (1 << 30))
                throw std::runtime_error("buffer size too large");
            if (size < 2)
                throw std::runtime_error("buffer size too small");
            if ((size & (size - 1)) != 0)
                throw std::runtime_error("buffer size is not power of 2");

            for (size_t i = 0; i != size; ++i)
                buffer[i].m_sequence.store(i, std::memory_order_relaxed);

            epos.store(0, std::memory_order_relaxed);
            dpos.store(0, std::memory_order_relaxed);
        }

        bool push_back(T data)
        {
            cell_t *cell;
            size_t curent_pos;
            bool result = false;

            while (!result)
            {
                curent_pos = epos.load(std::memory_order_acquire);
                cell = &buffer[curent_pos & buffer_mask];
                auto sequence = cell->m_sequence.load();
                auto diff = static_cast<int>(sequence) - static_cast<int>(curent_pos);

                if (diff < 0)
                    return false;

                if (diff == 0)
                    result = epos.compare_exchange_weak(curent_pos, curent_pos + 1, std::memory_order_relaxed);
            }

            cell->m_data = std::move(data);
            cell->m_sequence.store(curent_pos + 1, std::memory_order_release);
            return true;
        }

        bool pop(T &data)
        {
            cell_t *cell;
            size_t curent_pos;
            bool result = false;

            while (!result)
            {
                curent_pos = dpos.load(std::memory_order_relaxed);
                cell = &buffer[curent_pos & buffer_mask];
                auto sequence = cell->m_sequence.load(std::memory_order_acquire);
                auto diff = static_cast<int>(sequence) - static_cast<int>(curent_pos + 1);

                if (diff < 0)
                    return false;

                if (diff == 0)
                    result = dpos.compare_exchange_weak(curent_pos, curent_pos + 1, std::memory_order_relaxed);
            }

            data = std::move(cell->m_data);
            cell->m_sequence.store(curent_pos + buffer_mask + 1, std::memory_order_release);
            return true;
        }

        bool is_empty() const { return epos.load() == dpos.load(); }

        size_t available_space() const
        {
            auto enq = epos.load(std::memory_order_relaxed);
            auto deq = dpos.load(std::memory_order_relaxed);
            return buffer.size() - (enq - deq);
        }

        size_t size() const
        {
            auto enq = epos.load(std::memory_order_relaxed);
            auto deq = dpos.load(std::memory_order_relaxed);
            return enq - deq;
        }

        size_t capacity() const { return buffer.size(); }

        void push_back_blocking(T data)
        {
            while (!push_back(data))
                std::this_thread::yield();
        }
    };
} // namespace pot::algorithms