#pragma once

#include <atomic>
#include <coroutine>
#include <utility>

#include "pot/algorithms/lfqueue.h"

namespace pot::sync
{
    class async_lock
    {
    public:
        async_lock(size_t max_waiters = 1024) 
            : waiters(max_waiters) {}

        async_lock(const async_lock &) = delete;
        async_lock &operator=(const async_lock &) = delete;

        struct scoped_lock
        {
            scoped_lock() noexcept : m_lock(nullptr) {}
            explicit scoped_lock(async_lock *al) noexcept : m_lock(al) {}

            scoped_lock(scoped_lock &&other) noexcept : m_lock(other.m_lock)
            {
                other.m_lock = nullptr;
            }

            scoped_lock &operator=(scoped_lock &&other) noexcept
            {
                if (this != &other)
                {
                    release();
                    m_lock = other.m_lock;
                    other.m_lock = nullptr;
                }
                return *this;
            }

            scoped_lock(const scoped_lock &) = delete;
            scoped_lock &operator=(const scoped_lock &) = delete;

            ~scoped_lock() noexcept { release(); }

            explicit operator bool() const noexcept { return m_lock != nullptr; }

        private:
            void release() noexcept
            {
                if (!m_lock) return;
                m_lock->release_and_resume_next();
                m_lock = nullptr;
            }

            async_lock *m_lock;
        };

        auto lock() noexcept
        {
            struct awaitable
            {
                async_lock &parent;

                bool await_ready() const noexcept { return false; }

                bool await_suspend(std::coroutine_handle<> h) noexcept
                {
                    bool expected = false;
                    if (parent.m_locked.compare_exchange_strong(
                            expected, true, std::memory_order_acquire))
                    {
                        return false;
                    }

                    if (!parent.waiters.push_back(h))
                    {
                        throw std::runtime_error("async_lock waiters queue overflow");
                    }
                    return true;
                }

                scoped_lock await_resume() noexcept { return scoped_lock{&parent}; }
            };

            return awaitable{*this};
        }

        bool try_lock() noexcept
        {
            bool expected = false;
            return m_locked.compare_exchange_strong(expected, true, std::memory_order_acquire);
        }

    private:
        void release_and_resume_next() noexcept
        {
            std::coroutine_handle<> next{};
            if (!waiters.pop(next))
            {
                m_locked.store(false, std::memory_order_release);
                return;
            }

            if (next) next.resume();
        }

        std::atomic<bool> m_locked{false};
        pot::algorithms::lfqueue<std::coroutine_handle<>> waiters;
    };
} // namespace pot::sync
