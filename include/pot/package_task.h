#pragma once

#include "future.h"

namespace pot
{
    template <typename T>
    class packaged_task
    {
    public:
        template <typename Func>
        explicit packaged_task(Func &&func)
            : func(std::forward<Func>(func)), state(std::make_shared<shared_state<T>>()) {}

        future<T> get_future()
        {
            future<T> future;
            future.set_state(state);
            return future;
        }

        void operator()()
        {
            try
            {
                state->set_value(func());
            }
            catch (...)
            {
                state->set_exception(std::current_exception());
            }
        }

    private:
        std::function<T()> func;
        std::shared_ptr<shared_state<T>> state;
    };
}