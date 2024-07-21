#pragma once

#include "future.h"

namespace pot
{
    template <typename T>
    class promise
    {
    public:
        promise() : state(std::make_shared<shared_state<T>>()) {}

        future<T> get_future()
        {
            future<T> future;
            future.set_state(state);
            return future;
        }

        void set_value(const T &value)
        {
            state->set_value(value);
        }

        void set_exception(std::exception_ptr eptr)
        {
            state->set_exception(eptr);
        }

    private:
        std::shared_ptr<shared_state<T>> state;
    };
}