#pragma once

#include <atomic>
#include <memory>
#include <exception>
#include <stdexcept>
#include <chrono>

#include "pot/this_thread.h"

namespace pot
{
    template <typename T>
    class packaged_task;

    template <typename T>
    class promise;

    template <typename T>
    class shared_state
    {
    public:
        shared_state() : m_ready(false), m_value(nullptr) {}

        void set_value(const T &value)
        {
            auto tmp_value = std::make_shared<T>(value);
            bool expected = false;
            if (m_ready.compare_exchange_strong(expected, true, std::memory_order_release))
            {
                m_value.store(tmp_value, std::memory_order_release);
            }
            else
            {
                throw std::runtime_error("Value already set!");
            }
        }

        void set_exception(std::exception_ptr eptr)
        {
            bool expected = false;
            if (m_ready.compare_exchange_strong(expected, true, std::memory_order_release))
            {
                m_eptr = eptr;
            }
            else 
            {
                throw std::runtime_error("Exception already set!");
            }
        }

        T get()
        {
            wait();
            if (m_eptr)
            {
                std::rethrow_exception(m_eptr);
            }
            return *(m_value.load(std::memory_order_acquire));
        }

        void wait()
        {
            while (!m_ready.load(std::memory_order_acquire))
            {
                pot::this_thread::yield();
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
        std::atomic<bool> m_ready;
        std::atomic<std::shared_ptr<T>> m_value;
        std::exception_ptr m_eptr;
    };

    template <typename T>
    class future
    {
    public:
        future() = default;
        future(future &&other) noexcept : m_state(std::move(other.m_state)) {}
        future &operator=(future &&other) noexcept
        {
            if (this != &other)
            {
                m_state = std::move(other.m_state);
            }
            return *this;
        }

        T get()
        {
            if (!m_state)
                throw std::runtime_error("Future not valid!");
            return m_state->get();
        }

        void wait()
        {
            if (!m_state)
                throw std::runtime_error("Future not valid!");
            m_state->wait();
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period> &timeout_duration)
        {
            if (!m_state)
                throw std::runtime_error("Future not valid!");
            return m_state->wait_for(timeout_duration);
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time)
        {
            if (!m_state)
                throw std::runtime_error("Future not valid!");
            return m_state->wait_until(timeout_time);
        }

    private:
        std::shared_ptr<shared_state<T>> m_state;

        void set_state(std::shared_ptr<shared_state<T>> state)
        {
            m_state = state;
        }

        friend class packaged_task<T>;
        friend class promise<T>;
    };
}
