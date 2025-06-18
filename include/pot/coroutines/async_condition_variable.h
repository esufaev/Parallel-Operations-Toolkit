#pragma once 

#include <coroutine>
#include <atomic>

namespace pot::coroutines
{
    using coro_t = std::coroutine_handle<>;

    class async_condition_variable
    {
        struct awaiter;

        std::atomic<awaiter *> m_awaiter_head{nullptr};
        std::atomic<bool> m_set_state{false};

        struct awaiter
        {
            async_condition_variable &awaiting_event;
            coro_t coroutine_handle = nullptr;
            awaiter *next = nullptr;

            awaiter(async_condition_variable &event) noexcept : awaiting_event(event) {}

            bool await_ready() const noexcept { return awaiting_event.m_set_state.load(std::memory_order_acquire); }

            void await_suspend(coro_t c) noexcept
            {
                coroutine_handle = c;
                awaiter *old_head = awaiting_event.m_awaiter_head.load(std::memory_order_acquire);
                do { next = old_head; } 
                while (!awaiting_event.m_awaiter_head.compare_exchange_weak(
                    old_head, this, std::memory_order_release, std::memory_order_relaxed));
            }

            void await_resume() noexcept { awaiting_event.reset(); }
        };

    public:
        async_condition_variable(bool set = false) noexcept : m_set_state{set} {}

        async_condition_variable(const async_condition_variable &) = delete;
        async_condition_variable &operator=(const async_condition_variable &) = delete;
        async_condition_variable(async_condition_variable &&) = delete;
        async_condition_variable &operator=(async_condition_variable &&) = delete;

        awaiter operator co_await() noexcept { return awaiter{*this}; }

        void set() noexcept
        {
            m_set_state.store(true, std::memory_order_release);
            awaiter *head = m_awaiter_head.exchange(nullptr, std::memory_order_acquire);
            while (head != nullptr)
            {
                coro_t handle = head->coroutine_handle;
                head = head->next;
                handle.resume();
            }
        }

        void stop() noexcept
        {
            m_set_state.store(false, std::memory_order_release);
            m_awaiter_head.exchange(nullptr, std::memory_order_acquire);
        }

        bool is_set() const noexcept { return m_set_state.load(std::memory_order_acquire); }
        void reset() noexcept { m_set_state.store(false, std::memory_order_release); }
    };
}