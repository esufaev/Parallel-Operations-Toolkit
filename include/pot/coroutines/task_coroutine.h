#pragma once

#include <coroutine>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <source_location>
#include <chrono>
#include <type_traits>
#include <exception>

#include "pot/tasks/impl/shared_state.h"

namespace pot::coroutines
{
    template <typename T>
    class task;

    template <>
    class task<void>
    {
    public:
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        explicit task(std::shared_ptr<pot::tasks::details::shared_state<void>> state) noexcept
            : m_state(state) {}

        task(task&& rhs) noexcept
            : m_state(std::exchange(rhs.m_state, nullptr)) {}

        task& operator=(task&& rhs) noexcept
        {
            if (this != &rhs)
            {
                m_state = std::exchange(rhs.m_state, nullptr);
            }
            return *this;
        }

        bool is_ready() const noexcept
        {
            return m_state && m_state->is_ready();
        }

        void wait()
        {
            if (m_state)
                m_state->wait();
            else
                throw std::runtime_error("task<void>::wait called on empty task.");
        }

        void get()
        {
            if (m_state)
                m_state->wait();
            else
                throw std::runtime_error("task<void>::get called on empty task.");
        }

        bool await_ready() const noexcept
        {
            return is_ready();
        }

        void await_suspend(std::coroutine_handle<> h) const
        {
            h.resume();
        }

        void await_resume()
        {
            if (m_state)
                m_state->get();
        }

    private:
        std::shared_ptr<pot::tasks::details::shared_state<void>> m_state;
    };

    struct task<void>::promise_type
    {
        std::shared_ptr<pot::tasks::details::shared_state<void>> m_state = std::make_shared<pot::tasks::details::shared_state<void>>();

        task<void> get_return_object()
        {
            return task<void>{m_state};
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void()
        {
            m_state->set_value();
        }

        void unhandled_exception()
        {
            m_state->set_exception(std::current_exception());
        }
    };

    template <typename T>
    class promise
    {
    public:
        promise() : m_state(std::make_shared<pot::tasks::details::shared_state<T>>()) {}

        void set_value(const T& value)
        {
            if (m_state)
                m_state->set_value(value);
            else
                throw std::runtime_error("promise: set_value called on uninitialized state");
        }

        void set_value(T&& value)
        {
            if (m_state)
                m_state->set_value(std::move(value));
            else
                throw std::runtime_error("promise: set_value called on uninitialized state");
        }

        void set_exception(std::exception_ptr ex)
        {
            if (m_state)
                m_state->set_exception(ex);
            else
                throw std::runtime_error("promise: set_exception called on uninitialized state");
        }

        task<T> get_task()
        {
            return task<T>{m_state};
        }

    private:
        std::shared_ptr<pot::tasks::details::shared_state<T>> m_state;
    };

    template <>
    class promise<void>
    {
    public:
        promise() : m_state(std::make_shared<pot::tasks::details::shared_state<void>>()) {}

        void set_value()
        {
            if (m_state)
                m_state->set_value();
            else
                throw std::runtime_error("promise: set_value called on uninitialized state");
        }

        void set_exception(std::exception_ptr ex)
        {
            if (m_state)
                m_state->set_exception(ex);
            else
                throw std::runtime_error("promise: set_exception called on uninitialized state");
        }

        task<void> get_task()
        {
            return task<void>{m_state};
        }

    private:
        std::shared_ptr<pot::tasks::details::shared_state<void>> m_state;
    };

    template <typename T>
    class task
    {
    public:
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        task(handle_type h) noexcept
            : m_handle(h) {}

        task(task &&rhs) noexcept
            : m_handle(std::exchange(rhs.m_handle, nullptr)) {}

        task &operator=(task &&rhs) noexcept
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

        task(const task &) = delete;
        task &operator=(const task &) = delete;

        explicit operator bool() const noexcept
        {
            return m_handle && !m_handle.done();
        }

        auto get()
        {
            ensure_handle();
            if constexpr (std::is_void_v<T>)
            {
                m_handle.promise().m_state->get();
            }
            else
            {
                return m_handle.promise().m_state->get();
            }
        }

        void wait()
        {
            ensure_handle();
            m_handle.promise().m_state->wait();
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period> &timeout_duration)
        {
            ensure_handle();
            return m_handle.promise().m_state->wait_for(timeout_duration);
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time)
        {
            ensure_handle();
            return m_handle.promise().m_state->wait_until(timeout_time);
        }

        bool is_ready() const noexcept
        {
            ensure_handle();
            return m_handle.promise().m_state->is_ready();
        }

        bool await_ready() const noexcept
        {
            return is_ready();
        }

        void await_suspend(std::coroutine_handle<> h) const
        {
            h.resume();
        }

        auto await_resume()
        {
            if constexpr (!std::is_void_v<T>)
            {
                return get();
            }
            else
            {
                get();
            }
        }

        T next()
        {
            ensure_handle();
            if (m_handle && !m_handle.done())
            {
                m_handle.resume();
                return m_handle.promise().get_value();
            }
            throw std::runtime_error(std::string(std::source_location::current().function_name()) + " - Task completed, no more values.");
        }

        struct promise_type
        {
            std::shared_ptr<pot::tasks::details::shared_state<T>> m_state = std::make_shared<pot::tasks::details::shared_state<T>>();

            task<T> get_return_object()
            {
                return task<T>{handle_type::from_promise(*this)};
            }

            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }

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

            std::optional<T> current_value;
        };

    private:
        handle_type m_handle;

        void ensure_handle(std::source_location location = std::source_location::current()) const
        {
            if (!m_handle)
                throw std::runtime_error(std::string(location.function_name()) + " - Attempted to use an empty task.");
        }
    };
}
