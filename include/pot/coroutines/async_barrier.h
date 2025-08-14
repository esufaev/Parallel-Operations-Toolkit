#pragma once

#include <coroutine>
#include <mutex>
#include <vector>
#include <atomic>

namespace pot::coroutines
{
    class async_barrier
    {
    public:
        explicit async_barrier(size_t expected)
            : expected_count(expected), current_count(0) {}

        struct barrier_awaiter
        {
            async_barrier &barrier;

            bool await_ready() const noexcept
            {
                return barrier.current_count.load(std::memory_order_acquire) >= barrier.expected_count;
            }

            void await_suspend(std::coroutine_handle<> h)
            {
                std::unique_lock<std::mutex> lock(barrier.awaiters_mutex);

                if (barrier.current_count.load(std::memory_order_acquire) >= barrier.expected_count)
                {
                    lock.unlock();
                    h.resume();
                    return;
                }

                barrier.awaiters.push_back(h);
            }

            void await_resume() noexcept {}
        };

        barrier_awaiter operator co_await() { return barrier_awaiter{*this}; }

        void set()
        {
            size_t count = current_count.fetch_add(1, std::memory_order_acq_rel) + 1;

            if (count == expected_count)
            {
                std::vector<std::coroutine_handle<>> to_resume;

                {
                    std::lock_guard<std::mutex> lock(awaiters_mutex);
                    to_resume.swap(awaiters);
                }

                for (auto h : to_resume)
                    h.resume();
            }
        }

    private:
        const size_t expected_count;
        std::atomic<size_t> current_count;

        std::mutex awaiters_mutex;
        std::vector<std::coroutine_handle<>> awaiters;
    };
}
