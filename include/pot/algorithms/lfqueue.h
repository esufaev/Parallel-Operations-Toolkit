#pragma once

#include <atomic>
#include <optional>

namespace pot::algorithms
{

const size_t MPSC_QUEUE_CAPACITY = 1024; // temp

template <typename T> class lfqueue
{
  public:
    lfqueue();
    ~lfqueue();

    [[nodiscard]] std::optional<T> pop() noexcept;
    [[nodiscard]] bool push(const T &msg) noexcept;

  private:
    struct cell_t
    {
        std::atomic<size_t> sequence;
        T data;
    };

    static constexpr size_t capacity = MPSC_QUEUE_CAPACITY;
    static constexpr size_t mask = capacity - 1;

    cell_t *buffer;

    alignas(64) std::atomic<size_t> enqueuePos{0};
    alignas(64) std::atomic<size_t> dequeuePos{0};
};

template <typename T> lfqueue<T>::lfqueue()
{
    static_assert((capacity > 0 && ((capacity & (capacity - 1)) == 0)),
                  "Capacity must be a power of 2");

    buffer = new cell_t[capacity];

    for (size_t i = 0; i < capacity; ++i)
    {
        buffer[i].sequence.store(i, std::memory_order_relaxed);
    }
}

template <typename T> lfqueue<T>::~lfqueue() { delete[] buffer; }

template <typename T> bool lfqueue<T>::push(const T &msg) noexcept
{
    cell_t *cell;
    size_t pos = enqueuePos.load(std::memory_order_relaxed);

    while (true)
    {
        cell = &buffer[pos & mask];
        size_t seq = cell->sequence.load(std::memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)pos;

        if (dif == 0)
        {
            if (enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
            {
                cell->data = msg;

                cell->sequence.store(pos + 1, std::memory_order_release);
                return true;
            }
        }
        else if (dif < 0)
        {
            return false;
        }
        else
        {
            pos = enqueuePos.load(std::memory_order_relaxed);
        }
    }
}

template <typename T> std::optional<T> lfqueue<T>::pop() noexcept
{
    cell_t *cell;
    size_t pos = dequeuePos.load(std::memory_order_relaxed);

    while (true)
    {
        cell = &buffer[pos & mask];
        size_t seq = cell->sequence.load(std::memory_order_acquire);

        intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);

        if (dif == 0)
        {
            if (dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
            {
                auto result = std::move(cell->data);

                cell->sequence.store(pos + mask + 1, std::memory_order_release);

                return std::make_optional(std::move(result));
            }
        }
        else if (dif < 0)
        {
            return std::nullopt;
        }
        else
        {
            pos = dequeuePos.load(std::memory_order_relaxed);
        }
    }
}

} // namespace pot::algorithms
