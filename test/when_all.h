#pragma once

#include <iterator>
#include <memory>
#include <vector>

#include "pot/coroutines/test_task.h"

namespace pot::coroutines
{
/**
 * @brief Await multiple coroutine tasks and resume once all have completed.
 *
 * `when_all` provides a convenient way to wait for a collection or a pack of
 * coroutine tasks. Each task is awaited sequentially, ensuring that all tasks
 * complete before returning.
 *
 * @tparam Iterator Forward iterator type yielding coroutine tasks.
 * @tparam Container Container type holding coroutine tasks.
 * @tparam FuturePtr Pointer-like or task type stored in container.
 * @tparam Futures Variadic list of coroutine tasks.
 *
 * @param begin Iterator to the beginning of a range of tasks.
 * @param end   Iterator to the end of a range of tasks.
 *
 * @return task<void> A coroutine that completes once all provided tasks finish.
 */
template <typename Iterator>
    requires std::forward_iterator<Iterator>
pot::coroutines::task<void> when_all(Iterator begin, Iterator end)
{
    for (auto it = begin; it != end; ++it)
    {
        co_await std::move(*it);
    }
    co_return;
}

/**
 * @brief Await multiple coroutine tasks and resume once all have completed.
 *
 * `when_all` provides a convenient way to wait for a collection or a pack of
 * coroutine tasks. Each task is awaited sequentially, ensuring that all tasks
 * complete before returning.
 *
 * @tparam Iterator Forward iterator type yielding coroutine tasks.
 * @tparam Container Container type holding coroutine tasks.
 * @tparam FuturePtr Pointer-like or task type stored in container.
 * @tparam Futures Variadic list of coroutine tasks.
 *
 * @param futures Container of coroutine tasks.
 *
 * @return task<void> A coroutine that completes once all provided tasks finish.
 */
template <template <class, class...> class Container, typename FuturePtr, typename... OtherTypes>
pot::coroutines::task<void> when_all(Container<FuturePtr, OtherTypes...> &futures)
{
    co_return co_await when_all(std::begin(futures), std::end(futures));
}

/**
 * @brief Await multiple coroutine tasks and resume once all have completed.
 *
 * `when_all` provides a convenient way to wait for a collection or a pack of
 * coroutine tasks. Each task is awaited sequentially, ensuring that all tasks
 * complete before returning.
 *
 * @tparam Iterator Forward iterator type yielding coroutine tasks.
 * @tparam Container Container type holding coroutine tasks.
 * @tparam FuturePtr Pointer-like or task type stored in container.
 * @tparam Futures Variadic list of coroutine tasks.
 *
 * @param futures... Variadic coroutine tasks to be awaited.
 *
 * @return task<void> A coroutine that completes once all provided tasks finish.
 */
template <typename... Futures> pot::coroutines::task<void> when_all(Futures &&...futures)
{
    (co_await std::forward<Futures>(futures), ...);
    co_return;
}
} // namespace pot::coroutines
