#pragma once

#include "pot/future.h"

namespace pot
{
    template <typename T>
    class stack_promise
    {
    public:
        stack_promise() noexcept : m_state(std::make_shared<future<T>>()) {}

        std::shared_ptr<future<T>> get_future()
        {
            return m_state;
        }

        void set_value(const T &value)
        {
            m_state->set_value(value);
        }

        void set_exception(std::exception_ptr eptr)
        {
            m_state->set_exception(eptr);
        }

    private:
        std::shared_ptr<future<T>> m_state;
    };
}