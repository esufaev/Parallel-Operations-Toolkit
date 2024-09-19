#pragma once

#include <atomic>
#include <exception>
#include <chrono>
#include <variant>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <type_traits>

#include "pot/this_thread.h"

namespace pot::tasks::details
{
    template <typename T>
    class shared_state
    {
    public:
        using variant_type = std::conditional_t<std::is_void_v<T>,
            std::variant<std::monostate, std::exception_ptr>,
            std::variant<std::monostate, T, std::exception_ptr>>;

        shared_state() = default;

        template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
        void set_value(const U& value)
        {
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("shared_state::set_value() - value already set.");
            }

            m_variant = value;
            notify_completion();
        }

        template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
        void set_value(U&& value)
        {
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("shared_state::set_value() - value already set.");
            }

            m_variant = std::move(value);
            notify_completion();
        }

        template <typename U = T, typename = std::enable_if_t<std::is_void_v<U>>>
        void set_value()
        {
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("shared_state<void>::set_value() - value already set.");
            }

            m_variant = std::monostate{};
            notify_completion();
        }

        void set_exception(std::exception_ptr eptr)
        {
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("shared_state::set_exception() - exception already set.");
            }

            m_variant = eptr;
            notify_completion();
        }

        template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
        U get()
        {
            wait();
            if (std::holds_alternative<std::exception_ptr>(m_variant))
            {
                std::rethrow_exception(std::get<std::exception_ptr>(m_variant));
            }
            if (std::holds_alternative<std::monostate>(m_variant))
            {
                throw std::runtime_error("shared_state::get() - no value set.");
            }
            return std::get<U>(m_variant);
        }

        template <typename U = T, typename = std::enable_if_t<std::is_void_v<U>>>
        void get()
        {
            wait();
            if (std::holds_alternative<std::exception_ptr>(m_variant))
            {
                std::rethrow_exception(std::get<std::exception_ptr>(m_variant));
            }
        }

        void wait()
        {
            while (!m_ready.load(std::memory_order_acquire))
            {
                pot::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& timeout_duration)
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
        bool wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time)
        {
            return wait_for(timeout_time - std::chrono::steady_clock::now());
        }

        bool is_ready() noexcept
        {
            return m_ready.load();
        }

    private:
        void notify_completion()
        {
            m_ready.store(true, std::memory_order_acq_rel);
        }

        std::atomic<bool> m_ready{ false };
        variant_type m_variant{ std::monostate{} };
    };
}
