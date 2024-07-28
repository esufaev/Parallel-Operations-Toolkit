#pragma once

#include <atomic>
#include <exception>
#include <chrono>
#include <variant>

#include "pot/this_thread.h"

namespace pot::tasks::details
{
    template <typename T>
    class shared_state
    {
    public:
        using variant_type = std::variant<std::monostate, T, std::exception_ptr>;

        shared_state() : m_ready(false), m_variant(std::monostate{}) {}

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
            if (std::holds_alternative<std::exception_ptr>(m_variant))
            {
                std::rethrow_exception(std::get<std::exception_ptr>(m_variant));
            }
            return std::get<T>(m_variant);
        }

        void wait()
        {
            while (!m_ready.load(std::memory_order_acquire))
            {
                pot::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period> &timeout_duration)
        {
            auto start_time = std::chrono::steady_clock::now();
            while (!m_ready.load(std::memory_order_acquire))
            {
                if (std::chrono::steady_clock::now() - start_time > timeout_duration)
                {
                    return false;
                }
                pot::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return true;
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time)
        {
            return wait_for(timeout_time - std::chrono::steady_clock::now());
        }

    private:
        std::atomic<bool> m_ready;
        variant_type m_variant;
    };
}
