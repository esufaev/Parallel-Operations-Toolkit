#pragma once

#include <memory>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#include <functional>
#include <coroutine>
#include <optional>

#include "pot/tasks/impl/shared_state.h"

namespace pot::tasks
{
    template <typename T>
    class task
    {
    public:
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        task(handle_type h) noexcept : m_handle(h) {}
        task(task&& rhs) noexcept : m_handle(std::exchange(rhs.m_handle, nullptr)) {}
        
        task& operator=(task&& rhs) noexcept
        {
            if (this != &rhs)
            {
                if (m_handle)
                    m_handle.destroy();
                m_handle = std::exchange(rhs.m_handle, nullptr);
            }
            return *this;
        }

        ~task()
        {
            if (m_handle)
                m_handle.destroy();
        }

        task(const task&) = delete;
        task& operator=(const task&) = delete;

        explicit operator bool() const noexcept
        {
            return m_handle && !m_handle.done();
        }

        T get()
        {
            ensure_handle();
            m_handle.promise().m_state->wait();
            return m_handle.promise().m_state->get();
        }

        void wait()
        {
            ensure_handle();
            m_handle.promise().m_state->wait();
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& timeout_duration)
        {
            ensure_handle();
            return m_handle.promise().m_state->wait_for(timeout_duration);
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time)
        {
            ensure_handle();
            return m_handle.promise().m_state->wait_until(timeout_time);
        }

        bool is_ready() const noexcept
        {
            return m_handle && m_handle.promise().m_state->is_ready();
        }

        void on_completion(typename details::shared_state<T>::completion_callback callback) const
        {
            ensure_handle();
            m_handle.promise().m_state->on_completion(std::move(callback));
        }

        bool await_ready() const noexcept
        {
            return m_handle.promise().m_state->is_ready();
        }

        void await_suspend(std::coroutine_handle<> h) const
        {
            m_handle.promise().m_state->on_completion([h]() { h.resume(); });
        }

        T await_resume()
        {
            return get();
        }

        T next()
        {
            if (m_handle && !m_handle.done())
            {
                m_handle.resume();
                return m_handle.promise().get_value();
            }
            throw std::runtime_error("pot::tasks::task::next() - Task completed, no more values.");
        }

    private:
        handle_type m_handle;

        void ensure_handle() const
        {
            if (!m_handle)
                throw std::runtime_error("pot::tasks::task::esure_handle() - Attempted to use an empty task.");
        }

    public:
        struct promise_type
        {
            std::shared_ptr<details::shared_state<T>> m_state = std::make_shared<details::shared_state<T>>();
            std::optional<T> current_value;

            task get_return_object()
            {
                return task{handle_type::from_promise(*this)};
            }

            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }

            std::suspend_always yield_value(T value) noexcept
            {
                current_value = std::move(value);
                return {};
            }

            void return_value(T value)
            {
                m_state->set_value(std::move(value));
            }

            void unhandled_exception()
            {
                m_state->set_exception(std::current_exception());
            }

            T get_value()
            {
                return *current_value;
            }
        };
    };

    template <typename T>
    class promise
    {
    public:
        promise() : m_state(std::make_shared<details::shared_state<T>>()) {}

        task<T> get_future()
        {
            return task<T>{m_state};
        }

        void set_value(T value)
        {
            ensure_state();
            m_state->set_value(std::move(value));
        }

        void set_exception(std::exception_ptr eptr)
        {
            ensure_state();
            m_state->set_exception(eptr);
        }

    private:
        std::shared_ptr<details::shared_state<T>> m_state;

        void ensure_state() const
        {
            if (!m_state)
                throw std::runtime_error("pot::tasks::promise::ensure_state() - state is empty; cannot set value/exception.");
        }
    };
}
