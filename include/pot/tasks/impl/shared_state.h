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

        shared_state(const shared_state& other)
        {
            m_variant = other.m_variant;
            m_ready.store(other.m_ready.load(std::memory_order_acquire), std::memory_order_release);
        }

        shared_state& operator=(const shared_state& other)
        {
            if (this != &other)
            {
                m_variant = other.m_variant;
                m_ready.store(other.m_ready.load(std::memory_order_acquire), std::memory_order_release);
            }
            return *this;
        }

        template<typename U = T>
        void set_value() requires (std::is_void_v<U>)
        {
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("shared_state<void>::set_value() - value already set.");
            }

            m_variant = std::monostate{};
            notify_completion();
        }

        template<typename U = T>
        void set_value(U&& value) requires (!std::is_void_v<U>)
        {
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("shared_state::set_value() - value already set.");
            }

            m_variant.template emplace<T>(std::forward<U>(value));
            // m_variant = std::variant<T>(std::forward<U>(value));
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

        template <typename U = T>
            requires (!std::is_void_v<U>)
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

        template <typename U = void>
            requires (std::is_void_v<U>)
        void get()
        {
            wait();
            if (std::holds_alternative<std::exception_ptr>(m_variant))
            {
                std::rethrow_exception(std::get<std::exception_ptr>(m_variant));
            }
        }

        void wait() const
        {
            while (!m_ready.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& timeout_duration)
        {
            const auto start_time = std::chrono::steady_clock::now();
            while (!m_ready.load(std::memory_order_acquire))
            {
                if (std::chrono::steady_clock::now() - start_time > timeout_duration)
                {
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return true;
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time)
        {
            return wait_for(timeout_time - std::chrono::steady_clock::now());
        }

        bool is_ready() const noexcept
        {
            return m_ready.load();
        }

    private:
        void notify_completion()
        {
            // m_ready.store(true, std::memory_order_acq_rel);
            m_ready = true;
        }

        std::atomic<bool> m_ready{ false };
        variant_type m_variant{ std::monostate{} };
    };
}
