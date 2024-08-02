#pragma once

#include <memory>
#include <chrono>

#include "pot/tasks/impl/shared_state.h"
#include "pot/utils/errors.h"
#include "pot/tasks/impl/consts.h"

namespace pot::tasks
{
    template <typename T>
    class task
    {
    public:
        task() noexcept = default;
        task(task &&rhs) noexcept = default;
        task(details::shared_state<T> *state) : m_state(state) {}

        task &operator=(task &&rhs) noexcept
        {
            if (this != &rhs)
            {
                m_state = std::move(rhs.m_state);
            }

            return *this;
        }

        task(const task &rhs) = delete;
        task &operator=(const task &rhs) = delete;

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(m_state);
        }

        T get()
        {
            throw_if_empty(pot::details::consts::task_get_error_msg);
            return m_state->get();
        }

        void wait()
        {
            throw_if_empty(pot::details::consts::task_wait_error_msg);
            m_state->wait();
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period> &timeout_duration)
        {
            throw_if_empty(pot::details::consts::task_wait_for_error_msg);
            return m_state->wait_for(timeout_duration);
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time)
        {
            throw_if_empty(pot::details::consts::task_wait_until_error_msg);
            return m_state->wait_until(timeout_time);
        }

    private:
        details::shared_state<T> *m_state;

        void throw_if_empty(const char *message) const
        {
            if (static_cast<bool>(!m_state))
            {
                throw errors::empty_result(message);
            }
        }
    };
}
