#pragma once

#include <atomic>
#include <exception>
#include <functional>
#include <variant>
#include <chrono>
#include <thread>

#include "pot/this_thread.h"
#include "pot/utils/errors.h"
#include "pot/tasks/impl/consts.h"

namespace pot::tasks::details
{
    template <typename T>
    class lazy_promise;

    template <typename T>
    class lazy_shared_state
    {
    public:
        using variant_type = std::variant<std::monostate, T, std::exception_ptr>;

        explicit lazy_shared_state(std::function<T()> func)
            : m_func(std::move(func)), m_ready(false), m_variant(std::monostate{}) {}

        void run()
        {
            bool expected = false;
            if (m_ready.compare_exchange_strong(expected, true, std::memory_order_acquire))
            {
                try
                {
                    m_variant = m_func();
                }
                catch (...)
                {
                    m_variant = std::current_exception();
                }
            }
        }

        T get()
        {
            run();
            if (std::holds_alternative<T>(m_variant))
            {
                return std::get<T>(m_variant);
            }
            else if (std::holds_alternative<std::exception_ptr>(m_variant))
            {
                std::rethrow_exception(std::get<std::exception_ptr>(m_variant));
            }
            throw pot::errors::empty_result(pot::details::consts::shared_task_get_error_msg);
        }

        void wait()
        {
            while (!m_ready.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return true;
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time)
        {
            return wait_for(timeout_time - std::chrono::steady_clock::now());
        }

        void set_value(const T &value)
        {
            bool expected = false;
            if (m_ready.compare_exchange_strong(expected, true, std::memory_order_acquire))
            {
                m_variant = value;
            }
        }

        void set_exception(std::exception_ptr exception)
        {
            bool expected = false;
            if (m_ready.compare_exchange_strong(expected, true, std::memory_order_acquire))
            {
                m_variant = exception;
            }
        }

        bool is_ready() const
        {
            return m_ready.load(std::memory_order_acquire);
        }

    private:
        std::function<T()> m_func;
        std::atomic<bool> m_ready;
        variant_type m_variant;

        friend class lazy_promise<T>;
    };
}
