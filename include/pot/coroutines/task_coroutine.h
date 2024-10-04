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
    class task
    {
    public:
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        explicit task(std::shared_ptr<pot::tasks::details::shared_state<T>> state)
            : m_state(std::move(state)), m_handle(nullptr)
        {
        }

        task(handle_type h) noexcept
            : m_handle(h) {}

        task(handle_type h, std::shared_ptr<pot::tasks::details::shared_state<T>> state) noexcept
            : m_state(std::move(state)), m_handle(h)
        {
            h.promise().m_state = m_state;
        }

        task(task &&rhs) noexcept
            : m_handle(std::exchange(rhs.m_handle, nullptr)), m_state(std::move(rhs.m_state))
        {
        }

        task &operator=(task &&rhs) noexcept
        {
            if (this != &rhs)
            {
                if (m_handle && m_handle.done())
                {
                    m_handle.destroy();
                }
                m_handle = std::exchange(rhs.m_handle, nullptr);
                m_state = std::move(rhs.m_state);
            }
            return *this;
        }

        ~task()
        {
            if (m_handle && m_handle.done())
            {
                m_handle.destroy();
            }
        }

        void wait()
        {
            ensure_handle();
            m_handle.promise().m_state->wait();
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

        bool is_ready() const noexcept
        {
            return m_state && m_state->is_ready();
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
                return m_handle.promise().m_state->get_value();
            }
            throw std::runtime_error(std::string(std::source_location::current().function_name()) + " - Task completed, no more values.");
        }

    private:
        std::shared_ptr<pot::tasks::details::shared_state<T>> m_state;
        handle_type m_handle;

        void ensure_handle(std::source_location location = std::source_location::current()) const
        {
            if (!m_handle)
            {
                throw std::runtime_error(std::string(location.function_name()) + " - Attempted to use an empty task.");
            }
        }
    };

    template <typename T>
    struct task<T>::promise_type
    {
        std::shared_ptr<pot::tasks::details::shared_state<T>> m_state;

        promise_type() : m_state(std::make_shared<pot::tasks::details::shared_state<T>>()) {}

        task<T> get_return_object()
        {
            return task<T>{std::coroutine_handle<promise_type>::from_promise(*this)};
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
    };

    template <>
    struct task<void>::promise_type
    {
        std::shared_ptr<pot::tasks::details::shared_state<void>> m_state;

        promise_type() : m_state(std::make_shared<pot::tasks::details::shared_state<void>>()) {}

        task<void> get_return_object()
        {
            return task<void>{std::coroutine_handle<promise_type>::from_promise(*this)};
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

        template <typename U = T, typename std::enable_if<!std::is_void<U>::value>::type * = nullptr>
        void set_value(const U &value)
        {
            ensure_state();
            m_state->set_value(value);
        }

        template <typename U = T, typename std::enable_if<!std::is_void<U>::value>::type * = nullptr>
        void set_value(U &&value)
        {
            ensure_state();
            m_state->set_value(std::move(value));
        }

        template <typename U = T, typename std::enable_if<std::is_void<U>::value>::type * = nullptr>
        void set_value()
        {
            ensure_state();
            m_state->set_value();
        }

        void set_exception(std::exception_ptr ex)
        {
            ensure_state();
            m_state->set_exception(ex);
        }

        task<T> get_task()
        {
            return task<T>{m_state};
        }

    private:
        std::shared_ptr<pot::tasks::details::shared_state<T>> m_state;

        void ensure_state() const
        {
            if (!m_state)
            {
                throw std::runtime_error("promise: operation on an uninitialized state");
            }
        }
    };

} // namespace pot::coroutines
