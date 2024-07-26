#pragma once

#include <functional>
#include <memory>
#include "future.h"

namespace pot
{
    template <typename T>
    class packaged_task
    {
    public:
        template <typename Func>
        explicit packaged_task(Func &&func)
            : func(std::forward<Func>(func)), m_state(std::make_shared<future<T>>()) {}

        std::shared_ptr<future<T>> get_future()
        {
            return m_state;
        }

        void operator()()
        {
            try
            {
                m_state->set_value(func());
            }
            catch (...)
            {
                m_state->set_exception(std::current_exception());
            }
        }

    private:
        std::function<T()> func;
        std::shared_ptr<future<T>> m_state;
    };
}
