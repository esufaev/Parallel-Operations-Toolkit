#pragma once

#include <memory>
#include <chrono>
#include "pot/tasks/shared_state.h"

namespace pot::tasks
{
    template <typename T>
    class task
    {
    public:
        explicit task(details::shared_state<T> *state) : m_state(state) {}

        T get()
        {
            return m_state->get();
        }

        void wait()
        {
            m_state->wait();
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period> &timeout_duration)
        {
            return m_state->wait_for(timeout_duration);
        }

        template <typename Clock, typename Duration>
        bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout_time)
        {
            return m_state->wait_until(timeout_time);
        }

    private:
        details::shared_state<T> *m_state;
    };
}
