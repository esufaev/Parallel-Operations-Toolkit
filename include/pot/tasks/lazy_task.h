#pragma once

#include <variant>
#include <exception>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "pot/tasks/impl/lazy_shared_state.h"

namespace pot::tasks
{
    namespace details
    {
        enum class lazy_task_error_code
        {
            empty_result,
            lazy_task_failed,
            promise_already_satisfied,
            unknown_error
        };

        class lazy_task_exception : public std::runtime_error
        {
        public:
            lazy_task_exception(lazy_task_error_code code, const std::string &message)
                : std::runtime_error(message), error_code(code) {}

            lazy_task_error_code code() const noexcept
            {
                return error_code;
            }

        private:
            lazy_task_error_code error_code;
        };
    }

    template <typename T>
    class lazy_task
    {
    public:
        lazy_task() noexcept = default;

        explicit lazy_task(details::lazy_shared_state<T>* shared_state)
            : m_shared_state(shared_state)
        {
        }

        lazy_task(lazy_task&& rhs) noexcept
            : m_shared_state(rhs.m_shared_state)
        {
        }

        lazy_task& operator=(lazy_task&& rhs) noexcept
        {
            if (this != &rhs)
            {
                m_shared_state = rhs.m_shared_state;
            }
            return *this;
        }

        lazy_task(const lazy_task& rhs) = delete;
        lazy_task& operator=(const lazy_task& rhs) = delete;

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(m_shared_state);
        }

        T get()
        {
            if (!m_shared_state)
            {
                throw details::lazy_task_exception(details::lazy_task_error_code::lazy_task_failed, "pot::lazy_task::get() - result is empty.");
            }
            m_shared_state->run();
            return m_shared_state->get();
        }

        void wait()
        {
            if (!m_shared_state)
            {
                throw details::lazy_task_exception(details::lazy_task_error_code::lazy_task_failed, "pot::lazy_task::wait() - result is empty.");
            }
            m_shared_state->run();
            if (!m_shared_state->m_ready.load(std::memory_order_acquire))
            {
                throw details::lazy_task_exception(details::lazy_task_error_code::empty_result, "pot::lazy_task::wait() - result is empty. Task is not ready.");
            }
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& timeout_duration)
        {
            if (!m_shared_state)
            {
                throw details::lazy_task_exception(details::lazy_task_error_code::lazy_task_failed, "pot::lazy_task::wait_for() - result is empty.");
            }
            return m_shared_state->wait_for(timeout_duration);
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time)
        {
            if (!m_shared_state)
            {
                throw details::lazy_task_exception(details::lazy_task_error_code::lazy_task_failed, "pot::lazy_task::wait_until() - result is empty.");
            }
            return m_shared_state->wait_until(timeout_time);
        }

    private:
        details::lazy_shared_state<T>* m_shared_state;
    };

    template <typename T>
    class lazy_promise
    {
    public:
        lazy_promise() {}

        explicit lazy_promise(std::function<T()> func)
            : m_shared_state(new details::lazy_shared_state<T>(std::move(func))) {}

        ~lazy_promise()
        {
            delete m_shared_state;
        }

        lazy_promise(const lazy_promise&) = delete;
        lazy_promise& operator=(const lazy_promise&) = delete;

        lazy_task<T> get_future()
        {
            return lazy_task<T>(m_shared_state);
        }

        void set_value(const T& value)
        {
            if (m_shared_state)
            {
                m_shared_state->set_value(value);
            }
        }

        void set_exception(std::exception_ptr exception)
        {
            if (m_shared_state)
            {
                m_shared_state->set_exception(exception);
            }
        }

    private:
        details::lazy_shared_state<T>* m_shared_state = nullptr;
    };
}