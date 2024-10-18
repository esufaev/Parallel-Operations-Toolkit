#pragma once

#include <coroutine>
#include <atomic>
#include <queue>
#include <stdexcept>
#include <memory>
#include <functional>

namespace pot::coroutines
{
    class async_lock
    {
    public:
        async_lock() : m_locked(false) {}

        async_lock(const async_lock &) = delete;
        async_lock &operator=(const async_lock &) = delete;

        async_lock(async_lock &&other) noexcept
            : m_locked(other.m_locked.test_and_set())
        {
            if (!other.m_waiting_queue.empty())
            {
                m_waiting_queue = std::move(other.m_waiting_queue);
            }
        }

        async_lock &operator=(async_lock &&other) noexcept
        {
            if (this != &other)
            {
                m_locked.clear();
                if (!m_waiting_queue.empty())
                {
                    m_waiting_queue = std::move(other.m_waiting_queue);
                }
                m_locked.test_and_set();
            }
            return *this;
        }

        auto lock()
        {
            struct awaiter
            {
                async_lock &m_lock;
                std::coroutine_handle<> m_handle;

                bool await_ready() const noexcept
                {
                    return !m_lock.m_locked.test_and_set(std::memory_order_acquire);
                }

                void await_suspend(std::coroutine_handle<> handle) noexcept
                {
                    m_handle = handle;
                    m_lock.m_waiting_queue.push(handle);
                }

                void await_resume() noexcept {}
            };

            return awaiter{*this};
        }

        void unlock()
        {
            if (!m_waiting_queue.empty())
            {
                auto next_handle = m_waiting_queue.front();
                m_waiting_queue.pop();
                next_handle.resume();
            }
            else
            {
                m_locked.clear(std::memory_order_release);
            }
        }

    private:
        std::atomic_flag m_locked = ATOMIC_FLAG_INIT;
        std::queue<std::coroutine_handle<>> m_waiting_queue;
    };

    class lock_guard
    {
    public:
        explicit lock_guard(async_lock &lock)
            : m_lock(lock), m_owns_lock(true) {}

        ~lock_guard()
        {
            if (m_owns_lock)
            {
                m_lock.unlock();
            }
        }

        lock_guard(const lock_guard &) = delete;
        lock_guard &operator=(const lock_guard &) = delete;

        lock_guard(lock_guard &&other) noexcept
            : m_lock(other.m_lock), m_owns_lock(other.m_owns_lock)
        {
            other.m_owns_lock = false;
        }

        lock_guard &operator=(lock_guard &&other) noexcept
        {
            if (this != &other)
            {
                if (m_owns_lock)
                {
                    m_lock.unlock();
                }
                m_lock = std::move(other.m_lock);
                m_owns_lock = other.m_owns_lock;
                other.m_owns_lock = false;
            }
            return *this;
        }

    private:
        async_lock &m_lock;
        bool m_owns_lock;
    };
} // namespace pot::coroutines
