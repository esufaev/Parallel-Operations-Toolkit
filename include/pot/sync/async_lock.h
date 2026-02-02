#pragma once

#include <atomic>
#include <coroutine>
#include <mutex>
#include <queue>

namespace pot::sync
{

class async_lock
{
  public:
    class scoped_lock_guard
    {
      public:
        explicit scoped_lock_guard(async_lock &lock) : m_lock(&lock) {}

        ~scoped_lock_guard()
        {
            if (m_lock)
                m_lock->unlock();
        }

        scoped_lock_guard(const scoped_lock_guard &) = delete;
        scoped_lock_guard &operator=(const scoped_lock_guard &) = delete;

        scoped_lock_guard(scoped_lock_guard &&other) noexcept : m_lock(other.m_lock)
        {
            other.m_lock = nullptr;
        }

        scoped_lock_guard &operator=(scoped_lock_guard &&other) noexcept
        {
            if (this != &other)
            {
                if (m_lock)
                    m_lock->unlock();
                m_lock = other.m_lock;
                other.m_lock = nullptr;
            }
            return *this;
        }

      private:
        async_lock *m_lock;
    };

    struct lock_awaiter
    {
        async_lock &m_lock;

        bool await_ready() const noexcept
        {
            bool expected = false;
            return m_lock.m_is_locked.compare_exchange_strong(expected, true,
                                                              std::memory_order_acquire);
        }

        bool await_suspend(std::coroutine_handle<> h)
        {
            std::lock_guard guard(m_lock.m_queue_mutex);

            bool expected = false;
            if (m_lock.m_is_locked.compare_exchange_strong(expected, true,
                                                           std::memory_order_acquire))
            {
                return false;
            }

            m_lock.m_waiters.push(h);
            return true;
        }

        scoped_lock_guard await_resume() noexcept { return scoped_lock_guard{m_lock}; }
    };

    [[nodiscard]] lock_awaiter lock() { return lock_awaiter{*this}; }

  private:
    void unlock()
    {
        std::coroutine_handle<> next_handle = nullptr;

        {
            std::lock_guard guard(m_queue_mutex);
            if (m_waiters.empty())
            {
                m_is_locked.store(false, std::memory_order_release);
            }
            else
            {
                next_handle = m_waiters.front();
                m_waiters.pop();
            }
        }

        if (next_handle)
        {
            next_handle.resume();
        }
    }

    std::atomic<bool> m_is_locked{false};
    std::mutex m_queue_mutex;
    std::queue<std::coroutine_handle<>> m_waiters;
};

} // namespace pot::sync
