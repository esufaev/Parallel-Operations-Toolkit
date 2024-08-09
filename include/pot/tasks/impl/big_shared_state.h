#pragma once

#include <atomic>
#include <exception>
#include <chrono>
#include <variant>

#include "pot/this_thread.h"
#include "pot/tasks/impl/consts.h"
#include "pot/utils/errors.h"

namespace pot::tasks::details
{
    template <typename T>
    class shared_state
    {
    public:
        using variant_type = std::variant<std::monostate, T, std::exception_ptr>;

        shared_state() : m_ready(false), m_progress(0), m_variant(std::monostate{}), m_interrupted(false) {}

        void set_value(const T &value)
        {
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Value already set!");
            }

            m_variant = value;
        }

        void set_value(T &&value)
        {
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Value already set!");
            }

            m_variant = std::move(value);
        }

        void set_exception(std::exception_ptr eptr)
        {
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Exception already set!");
            }

            m_variant = eptr;
        }

        T get()
        {
            wait();
            if (std::holds_alternative<std::exception_ptr>(m_variant) or !m_interrupted.load())
            {
                std::rethrow_exception(std::get<std::exception_ptr>(m_variant));
            }
            return std::get<T>(m_variant);
        }

        void wait()
        {
            while (!m_ready.load(std::memory_order_acquire) && !m_interrupted.load())
            {
                pot::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (m_interrupted.load())
            {
                throw std::runtime_error("Task was interrupted");
            }
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period> &timeout_duration)
        {
            auto start_time = std::chrono::steady_clock::now();
            while (!m_ready.load(std::memory_order_acquire) && !m_interrupted.load())
            {
                if (std::chrono::steady_clock::now() - start_time > timeout_duration)
                {
                    return false;
                }
                pot::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (m_interrupted.load())
            {
                throw std::runtime_error("Task was cancelled");
            }
            return true;
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time)
        {
            return wait_for(timeout_time - std::chrono::steady_clock::now());
        }

        bool is_ready() noexcept
        {
            return m_ready.load();
        }

        void set_progress(int progress)
        {
            if (progress < 0 || progress > 100)
            {
                throw pot::errors::big_shared_state_progress(pot::details::consts::big_shared_state_set_progress_interrupted_error_msg);
            }
            if (m_interrupted.load())
            {
                throw pot::errors::iterrupted_error(pot::details::consts::big_shared_state_set_progress_interrupted_error_msg);
            }
            m_progress.store(progress, std::memory_order_release);
        }

        int get_progress() const noexcept
        {
            return m_progress.load(std::memory_order_acquire);
        }

        void interrupt()
        {
            m_interrupted.store(true, std::memory_order_release);
        }

        bool is_interrupted() const noexcept
        {
            return m_interrupted.load(std::memory_order_acquire);
        }

    private:
        std::atomic<bool> m_ready;
        std::atomic<int> m_progress;
        std::atomic<bool> m_interrupted;

        variant_type m_variant;
    };
}