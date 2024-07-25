#pragma once

#include "future.h"

namespace pot
{
    template <typename T>
    class packaged_task
    {
    public:
        using allocator_type = pot::allocators::shared_allocator<std::shared_ptr<shared_state<T>>>;

        template <typename Func>
        explicit packaged_task(Func &&func, const allocator_type& alloc = allocator_type())
            : func(std::forward<Func>(func)), state(std::make_shared<shared_state<T>>(alloc)) {}

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
