#pragma once

#include "pot/coroutines/task_coroutine.h"
#include <iterator>
#include <vector>

namespace pot
{
    template <typename Iterator>
    pot::coroutines::task<void> when_all(Iterator begin, Iterator end)
        requires std::forward_iterator<Iterator>
    {
        for (auto it = begin; it != end; ++it)
        {
            co_await std::move(*it);
        }

        co_return;
    }

    template <template <class, class...> class Container, typename FutureType, typename... OtherTypes>
    pot::coroutines::task<void> when_all(Container<FutureType, OtherTypes...> &futures)
    {
        co_await when_all(std::begin(futures), std::end(futures));
    }
} // namespace pot
