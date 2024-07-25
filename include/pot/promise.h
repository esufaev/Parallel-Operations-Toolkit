#pragma once

#include "future.h"

namespace pot
{
    template <typename T>
    class promise
    {
    public:
        using allocator_type = pot::allocators::shared_allocator<std::shared_ptr<shared_state<T>>>;

        promise(const allocator_type& alloc = allocator_type()) 
            : state(std::make_shared<shared_state<T>>(alloc)) {}

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
