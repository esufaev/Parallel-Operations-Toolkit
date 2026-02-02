module;

#include "pot/utils/platform.h"

#if defined(POT_COMPILER_MSVC)

import <iterator>
import <utility>
import <vector>
import <memory>
import <tuple>
import <coroutine>

#else

#include <iterator>
#include <utility>
#include <vector>
#include <memory>
#include <tuple>
#include <coroutine>

#endif

export module pot.coroutines.when_all;

import pot.coroutines.task;

export namespace pot::coroutines
{
    template <typename Iterator> requires std::forward_iterator<Iterator>
    pot::coroutines::task<void> when_all(Iterator begin, Iterator end)
    {
        for (auto it = begin; it != end; ++it) co_await std::move(*it);
        co_return;
    }

    template <template <class, class...> class Container, typename FuturePtr, typename... OtherTypes>
    pot::coroutines::task<void> when_all(Container<FuturePtr, OtherTypes...> &futures)
    {
        co_return co_await when_all(std::begin(futures), std::end(futures));
    }

    template <typename... Futures>
    pot::coroutines::task<void> when_all(Futures&&... futures)
    {
        (co_await std::forward<Futures>(futures), ...);
        co_return;
    }
}
