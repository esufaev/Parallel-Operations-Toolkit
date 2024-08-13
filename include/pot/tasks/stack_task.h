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
                throw std::runtime_error("pot::stack_task::get() - no value set.");
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
                throw std::runtime_error("pot::stack_task::set_value() - value already set.");
            }
            new (&m_value) T(value);
            m_has_value.store(true);
        }

        void set_value(T &&value)
        {
            if (m_ready.exchange(true))
            {
                throw std::runtime_error("pot::stack_task::set_value() - value already set.");
            }
            new (&m_value) T(std::move(value));
            m_has_value.store(true);
        }

        void set_exception(std::exception_ptr eptr)
        {
            if (m_ready.exchange(true))
            {
                throw std::runtime_error("pot::stack_task::set_exception() - exception already set.");
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
