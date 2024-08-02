#pragma once

#include <utility>
#include "pot/tasks/impl/lazy_shared_state.h"
#include "pot/tasks/lazy_task.h"

namespace pot::tasks
{
    template <typename T>
    class lazy_promise
    {
    public:
        lazy_promise()
            : m_shared_state(nullptr) {}

        explicit lazy_promise(std::function<T()> func)
            : m_shared_state(new details::lazy_shared_state<T>(std::move(func))) {}

        ~lazy_promise()
        {
            delete m_shared_state;
        }

        lazy_promise(const lazy_promise &) = delete;
        lazy_promise &operator=(const lazy_promise &) = delete;

        lazy_task<T> get_future()
        {
            return lazy_task<T>(m_shared_state);
        }

        void set_value(const T &value)
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
        details::lazy_shared_state<T> *m_shared_state;
    };
}
