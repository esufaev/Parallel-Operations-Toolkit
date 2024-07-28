#pragma once

#include <memory>
#include <exception>
#include <utility>
#include "pot/tasks/shared_state.h"
#include "pot/tasks/task.h"

namespace pot::tasks
{
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
