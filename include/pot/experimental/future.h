#pragma once

#include <atomic>
#include <exception>
#include <stdexcept>
#include <chrono>
#include <new>

#include "pot/this_thread.h"

namespace pot
{
    template <typename T>
    class packaged_task;

    template <typename T>
    class promise;

    template <typename T>
    class future
    {
    public:
        future() noexcept
            : m_ready(false), m_has_value(false), m_has_exception(false) {}

        future(future &&other) noexcept
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

        future &operator=(future &&other) noexcept
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
                throw std::runtime_error("No value set!");
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
                throw std::runtime_error("Value already set!");
            }

            new (&m_value) T(value);
            m_has_value.store(true);
        }

        void set_value(T &&value)
        {
            if (m_ready.exchange(true))
            {
                throw std::runtime_error("Value already set!");
            }

            new (&m_value) T(std::move(value));
            m_has_value.store(true);
        }

        void set_exception(std::exception_ptr eptr)
        {
            if (m_ready.exchange(true))
            {
                throw std::runtime_error("Exception already set!");
            }

            m_exception = eptr;
            m_has_exception.store(true);
        }

        friend class packaged_task<T>;
        friend class promise<T>;
    };
}
