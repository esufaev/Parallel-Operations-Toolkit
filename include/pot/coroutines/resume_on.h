#pragma once

#include "pot/executors/executor.h"

namespace pot::details
{
    class resume_on_awaitable
    {
        pot::executor &m_executor;
    public:
        explicit resume_on_awaitable(pot::executor &ex) noexcept : m_executor(ex) {}

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            m_executor.run_detached(h);
        }

        void await_resume() {}
    };
} // namespace pot::coroutines::details

namespace pot::coroutines
{
    auto resume_on(pot::executor &executor) noexcept
    {
        return details::resume_on_awaitable(executor);
    }
} // namespace pot::coroutines
