#pragma once

#include <coroutine>
#include <atomic>
#include <queue>
#include <stdexcept>
#include <memory>
#include <functional>

namespace pot::coroutines
{
    class async_lock {
    public:
        struct lock_guard {
            explicit lock_guard(async_lock& lock) noexcept : m_lock(lock) {}
            ~lock_guard() noexcept { m_lock.unlock(); }

            lock_guard(const lock_guard&) = delete;
            lock_guard& operator=(const lock_guard&) = delete;
            lock_guard(lock_guard&&) noexcept = default;
            lock_guard& operator=(lock_guard&&) noexcept = delete;

        private:
            async_lock& m_lock;
        };

        auto lock() noexcept
        {
            struct awaitable
            {
                async_lock& lock;

                explicit awaitable(async_lock& l) noexcept : lock(l) {}

                [[nodiscard]] bool await_ready() const noexcept {
                    std::lock_guard guard(lock.m_mutex);
                    return lock.m_waiting_handles.empty();
                }

                void await_suspend(std::coroutine_handle<> h) noexcept {
                    std::lock_guard guard(lock.m_mutex);
                    lock.m_waiting_handles.push(h);
                }

                lock_guard await_resume() noexcept {
                    return lock_guard(lock);
                }

            };

            return awaitable(*this);
        }

    private:
        void unlock() noexcept {
            std::coroutine_handle<> to_resume;
            {
                std::lock_guard guard(m_mutex);
                if (!m_waiting_handles.empty()) {
                    to_resume = m_waiting_handles.front();
                    m_waiting_handles.pop();
                }
            }
            if (to_resume) to_resume.resume();
        }

        std::queue<std::coroutine_handle<>> m_waiting_handles;
        std::mutex m_mutex;

        friend struct lock_guard;
    };
} // namespace pot::coroutines
