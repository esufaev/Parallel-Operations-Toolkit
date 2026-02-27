#pragma once

#include <atomic>
#include <coroutine>
#include <optional>
#include <thread>
#include <utility>

#include "pot/executors/executor.h"
#include "pot/algorithms/lfqueue.h" 

namespace pot::sync
{
  class async_lock
  {
    public:
        using queue_type = std::pair<std::coroutine_handle<>, pot::executor*>;

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
      async_lock& m_lock;
      pot::executor* m_executor;

      bool await_ready() const noexcept { return false; }

      bool await_suspend(std::coroutine_handle<> handle)
      {
          int prev_state = m_lock.m_state.fetch_add(1, std::memory_order_acq_rel);
          if (prev_state == 0)
          {
              return false; 
          }

          while (!m_lock.m_queue.push({handle, m_executor})) std::this_thread::yield();
          return true; 
      }

      scoped_lock_guard await_resume() noexcept { return scoped_lock_guard(m_lock); } 
    };

    [[nodiscard]] lock_awaiter lock(pot::executor* executor = nullptr)
    {
      return lock_awaiter{*this, executor};
    }

    private:
      std::atomic<int> m_state{0};      
      pot::algorithms::lfqueue<queue_type> m_queue;

    void unlock()
    {
        int prev_state = m_state.fetch_sub(1, std::memory_order_acq_rel);
        if (prev_state == 1) return;

        std::optional<queue_type> next_task;
        
        while (!(next_task = m_queue.pop())) std::this_thread::yield();

        auto [handle, executor] = *next_task;

        if (executor)
        {
            executor->run_detached([h = handle]() mutable { h.resume(); });
        }
        else
        {
            handle.resume();
        }
    }
  };
}
