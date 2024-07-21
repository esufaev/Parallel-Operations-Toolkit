#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <exception>
#include <stdexcept>
#include <chrono>
#include <iostream>

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
        explicit shared_state() : m_ready(false), m_value{} {}

        void set_value(const T &value)
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_ready)
                throw std::runtime_error("Value already set!");
            m_value = value;
            m_ready = true;
            m_cv.notify_all();
        }

        void set_exception(std::exception_ptr eptr)
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_ready)
                throw std::runtime_error("Exception already set!");
            m_eptr = eptr;
            m_ready = true;
            m_cv.notify_all();
        }

        T get()
        {
            wait();
            if (m_eptr)
            {
                std::rethrow_exception(m_eptr);
            }
            return m_value;
        }

        void wait()
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            m_cv.wait(lock, [this]
                      { return m_ready; });
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period> &timeout_duration)
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            return m_cv.wait_for(lock, timeout_duration, [this]
                                 { return m_ready; });
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time)
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            return m_cv.wait_until(lock, timeout_time, [this]
                                   { return m_ready; });
        }

    private:
        bool m_ready;

        T m_value;
        std::exception_ptr m_eptr;

        std::mutex m_mtx;
        std::condition_variable m_cv;
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
