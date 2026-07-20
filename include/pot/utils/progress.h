#pragma once

#include <mutex>
#include <string>
#include <atomic>

namespace pot::coroutines::details
{
    struct progress
    {
      std::atomic<int64_t> m_min_value{0};
      std::atomic<int64_t> m_max_value{0};
      std::atomic<int64_t> m_value{0};
      
      mutable std::mutex m_text_mtx;
      std::string m_text;

      progress() = default;

      progress(const progress& other) noexcept 
      {
          m_min_value.store(other.m_min_value.load(std::memory_order_relaxed), std::memory_order_relaxed);
          m_max_value.store(other.m_max_value.load(std::memory_order_relaxed), std::memory_order_relaxed);
          m_value.store(other.m_value.load(std::memory_order_relaxed), std::memory_order_relaxed);
          
          std::lock_guard lock(other.m_text_mtx);
          m_text = other.m_text;
      }

      progress& operator=(const progress& other) noexcept 
      {
          m_min_value.store(other.m_min_value.load(std::memory_order_relaxed), std::memory_order_relaxed);
          m_max_value.store(other.m_max_value.load(std::memory_order_relaxed), std::memory_order_relaxed);
          m_value.store(other.m_value.load(std::memory_order_relaxed), std::memory_order_relaxed);
          
          std::scoped_lock lock(m_text_mtx, other.m_text_mtx);
          m_text = other.m_text;
          return *this;
      }

      void set_progress_range(int64_t min_v, int64_t max_v) noexcept
      {
          m_min_value.store(min_v, std::memory_order_relaxed);
          m_max_value.store(max_v, std::memory_order_relaxed);
          
          int64_t current = m_value.load(std::memory_order_relaxed);
          if (current < min_v) m_value.store(min_v, std::memory_order_relaxed);
          if (current > max_v) m_value.store(max_v, std::memory_order_relaxed);
      }

      void set_progress_value(int64_t v) noexcept
      {
          int64_t min_v = m_min_value.load(std::memory_order_relaxed);
          int64_t max_v = m_max_value.load(std::memory_order_relaxed);
          if (v < min_v) v = min_v;
          if (v > max_v) v = max_v;
          
          m_value.store(v, std::memory_order_release);
      }

      void set_progress_value_and_text(int64_t v, const char* t) noexcept
      {
          {
              std::lock_guard lock(m_text_mtx);
              m_text = t;
          }
          set_progress_value(v); 
      }

      int64_t get_value() const noexcept { return m_value.load(std::memory_order_acquire); }
      int64_t get_max_value() const noexcept { return m_max_value.load(std::memory_order_relaxed); }
      
      std::string get_text() const 
      {
          std::lock_guard lock(m_text_mtx);
          return m_text;
      }

      inline bool is_finished() const noexcept
      {
          return get_value() >= get_max_value();
      }
  };
}
