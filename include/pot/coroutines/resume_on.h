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

    void await_suspend(std::coroutine_handle<> h) noexcept { m_executor.run_detached(h); }

    void await_resume() {}
};
} // namespace pot::details

namespace pot::coroutines
{
/**
 * @brief Returns an awaitable that resumes the awaiting coroutine on the given executor.
 *
 * @param executor Executor where the coroutine should continue execution.
 * @return An awaitable that, when awaited, schedules resumption on @p executor.
 *
 */
inline auto resume_on(pot::executor &executor) noexcept
{
    return pot::details::resume_on_awaitable(executor);
}
} // namespace pot::coroutines
