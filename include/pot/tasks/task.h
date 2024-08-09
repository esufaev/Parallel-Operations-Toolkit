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

    template <typename T, typename Allocator = std::allocator<T>>
    class promise
    {
    public:
        explicit promise(const Allocator &alloc = Allocator())
            : m_allocator(alloc)
        {
            m_state = allocate_shared_state();
        }

        ~promise()
        {
            if (m_state)
            {
                deallocate_shared_state(m_state);
            }
        }

        task<T> get_future()
        {
            return task<T>(m_state);
        }

        void set_value(const T &value)
        {
            m_state->set_value(value);
        }

        void set_value(T &&value)
        {
            m_state->set_value(std::move(value));
        }

        void set_exception(std::exception_ptr eptr)
        {
            m_state->set_exception(eptr);
        }

        details::shared_state<T> *get_state() const
        {
            return m_state;
        }

    private:
        details::shared_state<T> *allocate_shared_state()
        {
            auto ptr = m_allocator.allocate(1);
            return new (ptr) details::shared_state<T>();
        }

        void deallocate_shared_state(details::shared_state<T> *state)
        {
            state->~shared_state();
            m_allocator.deallocate(state, 1);
        }

        Allocator m_allocator;
        details::shared_state<T> *m_state;
    };
}
