#pragma once

#include <atomic>
#include <exception>
#include <stdexcept>
#include <chrono>
#include <new>
#include <string>
#include <utility>

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
        stack_task() noexcept : m_ready(false), m_has_value(false), m_has_exception(false) {}

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
                throw details::stack_exception(details::stack_error_code::no_value_set, "pot::task::stack_task::get() - no value set.");
            }
        }

        void wait()
        {
            while (!m_ready.load())
            {
                pot::this_thread::yield();
            }
        }

        bool wait_for(const std::chrono::milliseconds &duration)
        {
            auto start = std::chrono::steady_clock::now();
            while (!m_ready.load())
            {
                if (std::chrono::steady_clock::now() - start >= duration)
                {
                    return false;
                }
                pot::this_thread::yield();
            }
            return true;
        }

        bool wait_until(const std::chrono::steady_clock::time_point &timeout)
        {
            while (!m_ready.load())
            {
                if (std::chrono::steady_clock::now() >= timeout)
                {
                    return false;
                }
                pot::this_thread::yield();
            }
            return true;
        }

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

    private:
        std::atomic<bool> m_ready;
        std::atomic<bool> m_has_value;
        std::atomic<bool> m_has_exception;
        std::exception_ptr m_exception;

        alignas(T) unsigned char m_value_storage[sizeof(T)];
        T &m_value = *reinterpret_cast<T *>(&m_value_storage);
    };

    template <typename T>
    class stack_promise
    {
    public:
        stack_promise() noexcept {}

        stack_task<T> &get_future()
        {
            return m_task;
        }

        void set_value(const T &value)
        {
            m_task.set_value(value);
        }

        void set_value(T &&value)
        {
            m_task.set_value(std::move(value));
        }

        void set_exception(std::exception_ptr eptr)
        {
            m_task.set_exception(eptr);
        }

    private:
        stack_task<T> m_task;
    };
}