#pragma once

#include <iterator>
#include <vector>
#include <memory>

#include "pot/coroutines/task.h"

namespace pot::coroutines
{
    template <typename Iterator>
    pot::coroutines::lazy_task<void> when_all(Iterator begin, Iterator end)
        requires std::forward_iterator<Iterator>
    {
        for (auto it = begin; it != end; ++it)
        {
            co_await std::move(*(*it));
        }
        co_return;
    }

    template <template <class, class...> class Container, typename FuturePtr, typename... OtherTypes>
    pot::coroutines::task<void> when_all(Container<FuturePtr, OtherTypes...> &futures)
    {
        co_return co_await when_all(std::begin(futures), std::end(futures));
    }

} // namespace pot
