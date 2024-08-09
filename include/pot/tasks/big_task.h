#pragma once

#include <memory>
#include <chrono>
#include <atomic>
#include <iostream>

#include "pot/tasks/impl/big_shared_state.h"
#include "pot/utils/errors.h"
#include "pot/tasks/impl/consts.h"

namespace pot::tasks
{
    template <typename T>
    class big_task
    {
    public:
        big_task() noexcept : m_state(nullptr) {}
        explicit big_task(details::shared_state<T> *state)
            : m_state(state) {}

        big_task(big_task &&rhs) noexcept
            : m_state(rhs.m_state)
        {
            rhs.m_state = nullptr;
        }

        big_task &operator=(big_task &&rhs) noexcept
        {
            if (this != &rhs)
            {
                m_state = rhs.m_state;
                rhs.m_state = nullptr;
            }
            return *this;
        }

        big_task(const big_task &rhs) = delete;
        big_task &operator=(const big_task &rhs) = delete;

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

        void interrupt() noexcept
        {
            m_state->interrupt();  
        }

        bool is_interrupted() const noexcept
        {
            return m_state->is_interrupted();
        }

        void set_progress(int progress)
        {
            m_state->set_progress(progress); 
        }

        int get_progress() const noexcept
        {
            return m_state->get_progress();
        }

    private:
        details::shared_state<T> *m_state;

        void throw_if_empty(const char *message) const
        {
            if (!m_state)
            {
                throw errors::empty_result(message);
            }
        }
    };

    template <typename T>
    class big_task_promise
    {
    public:
        big_task_promise()
            : m_state(std::make_shared<details::shared_state<T>>()) {}

        big_task<T> get_task() noexcept
        {
            return big_task<T>(m_state.get());
        }

        void set_value(T value)
        {
            if (m_state->is_ready())
            {
                throw pot::errors::big_task_promise_already_satisfied(pot::details::consts::big_task_promise_already_satisfied_set_value_error_msg);
            }
            m_state->set_value(std::move(value));
        }

        void set_exception(std::exception_ptr e)
        {
            if (m_state->is_ready())
            {
                throw pot::errors::big_task_promise_already_satisfied(pot::details::consts::big_task_promise_already_satisfied_set_exception_error_msg);
            }
            m_state->set_exception(std::move(e));
        }

        void set_progress(int progress)
        {
            if (m_state->is_ready())
            {
                throw pot::errors::big_task_promise_already_satisfied("Cannot set progress. Task is already completed.");
            }
            m_state->set_progress(progress);
        }

        int get_progress() const noexcept
        {
            return m_state->get_progress();
        }

        void interrupt()
        {
            m_state->interrupt();
        }

        bool is_interrupted() const noexcept
        {
            return m_state->is_interrupted();
        }

        big_task_promise(const big_task_promise &) = delete;
        big_task_promise &operator=(const big_task_promise &) = delete;

    private:
        std::shared_ptr<details::shared_state<T>> m_state;
    };
}