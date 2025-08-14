#pragma once
#include <type_traits>

#include <pot/coroutines/task.h>

namespace pot::traits::concepts
{
    // template <typename T>
    // concept is_task = std::is_same_v<T, pot::coroutines::task<typename T::value_type>>;
    //
    // template <typename T>
    // concept is_not_task = !is_task<T>;

    template <typename ValueType, typename T>
    concept is_convertible_to = std::is_convertible_v<ValueType, T>;
}
