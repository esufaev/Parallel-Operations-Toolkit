#pragma once

#include <atomic>
#include <memory>

#include "pot/utils/cache_line.h"

namespace pot::coroutines::details
{
    struct progress; 

    struct task_meta
    {
        void* m_executor_ptr{nullptr};
        bool (*m_steal_func)(void*){nullptr};
        std::shared_ptr<pot::coroutines::details::progress> m_progress{nullptr};

        alignas(pot::cache_line_alignment) std::atomic<size_t> run_count{0};

        task_meta() = default;

        task_meta(const task_meta &other)
        {
            m_executor_ptr = other.m_executor_ptr;
            m_steal_func = other.m_steal_func;
            run_count.store(other.run_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            m_progress = other.m_progress;
        }

        task_meta &operator=(const task_meta &other)
        {
            m_executor_ptr = other.m_executor_ptr;
            m_steal_func = other.m_steal_func;
            run_count.store(other.run_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            m_progress = other.m_progress;
            return *this;
        }

        inline bool try_steal() const 
        {
            if (m_steal_func && m_executor_ptr)
                return m_steal_func(m_executor_ptr);
            return false;
        }
    };
} // namespace pot::coroutines::details
