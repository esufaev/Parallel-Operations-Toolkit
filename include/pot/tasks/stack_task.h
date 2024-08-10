#pragma once

#include <atomic>
#include <exception>
#include <stdexcept>
#include <chrono>
#include <new>
#include <string>

#include "pot/this_thread.h"

namespace pot::tasks
{
    namespace details
    {
        enum class stack_error_code
        {
            value_already_set,
            exception_already_set,
            no_value_set,
            unknown_error
        };

        class stack_exception : public std::runtime_error
        {
        public:
            stack_exception(stack_error_code code, const std::string &message)
                : std::runtime_error(message), error_code(code) {}

            stack_error_code code() const noexcept
            {
                return error_code;
            }

        private:
            stack_error_code error_code;
        };
    }

    template <typename T>
    class stack_task
    {
    public:
        stack_task() noexcept
            : m_ready(false), m_has_value(false), m_has_exception(false) {}

        stack_task(stack_task &&other) noexcept
            : m_ready(other.m_ready.load()), m_has_value(other.m_has_value.load()), m_has_exception(other.m_has_exception.load())
        {
            if (m_has_value)
            {
                new (&m_value) T(std::move(other.m_value));
            }
            else if (m_has_exception)
            {
                m_exception = other.m_exception;
            }
        }

        stack_task &operator=(stack_task &&other) noexcept
        {
            if (this != &other)
            {
                if (m_has_value)
                {
                    m_value.~T();
                }

                m_ready.store(other.m_ready.load());
                m_has_value.store(other.m_has_value.load());
                m_has_exception.store(other.m_has_exception.load());

                if (m_has_value)
                {
                    new (&m_value) T(std::move(other.m_value));
                }
                else if (m_has_exception)
                {
                    m_exception = other.m_exception;
                }
            }
            return *this;
        }

        T get()
        {
            wait();
            if (m_has_exception)
            {
                std::rethrow_exception(m_exception);
            }

            if (m_has_value)
            {
                return std::move(m_value);
            }
            else
            {
                throw details::stack_exception(stack_error_code::no_value_set, "pot::task::stack_task::get() - no value set.");
            }
        }

        void wait()
        {
            while (!m_ready.load())
            {
                pot::this_thread::yield();
            }
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period> &timeout_duration)
        {
            auto start_time = std::chrono::steady_clock::now();
            while (!m_ready.load())
            {
                if (std::chrono::steady_clock::now() - start_time > timeout_duration)
                {
                    return false;
                }
                pot::this_thread::yield();
            }
            return true;
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time)
        {
            return wait_for(timeout_time - std::chrono::steady_clock::now());
        }

    private:
        alignas(T) unsigned char m_value_storage[sizeof(T)];

        std::atomic<bool> m_ready;
        std::atomic<bool> m_has_value;
        std::atomic<bool> m_has_exception;
        std::exception_ptr m_exception;

        T &m_value = reinterpret_cast<T &>(m_value_storage);

        void set_value(const T &value)
        {
            if (m_ready.exchange(true))
            {
                throw details::stack_exception(details::stack_error_code::value_already_set, "pot::tasks::stack_task::set_value() - value already set.");
            }

            new (&m_value) T(value);
            m_has_value.store(true);
        }

        void set_value(T &&value)
        {
            if (m_ready.exchange(true))
            {
                throw details::stack_exception(details::stack_error_code::value_already_set, "pot::tasks::stack_task::set_value() - value already set.");
            }

            new (&m_value) T(std::move(value));
            m_has_value.store(true);
        }

        void set_exception(std::exception_ptr eptr)
        {
            if (m_ready.exchange(true))
            {
                throw details::stack_exception(details::stack_error_code::exception_already_set, "pot::tasks::stack_task::set_exception - exception already set.");
            }

            m_exception = eptr;
            m_has_exception.store(true);
        }

        friend class stack_promise<T>;
    };

    template <typename T>
    class stack_promise
    {
    public:
        stack_promise() noexcept : m_state(std::make_shared<stack_task<T>>()) {}

        std::shared_ptr<stack_task<T>> get_future()
        {
            return m_state;
        }

        void set_value(const T &value)
        {
            m_state->set_value(value);
        }

        void set_exception(std::exception_ptr eptr)
        {
            m_state->set_exception(eptr);
        }

    private:
        std::shared_ptr<stack_task<T>> m_state;
    };
}