#pragma once

#include <type_traits>

namespace pot::traits::compare
{
    template<typename T, typename... Ts>
        requires (sizeof...(Ts) > 0)
    [[nodiscard]] constexpr bool is_one_of(T&& value, Ts&&... values)
    {
        return ((value == values) || ...);
    }

    template<typename T, typename... Ts>
    constexpr bool is_one_of_type = (std::is_same_v<T, Ts> || ...);

    // template<typename T, typename... Ts>
    // struct is_one_of_type : std::disjunction(std::is_same_v<T, Ts>...) {};
    //
    // template<typename T, typename... Ts>
    // constexpr bool is_one_of_type_v = is_one_of_type<T, Ts...>::value;


    // template<typename ValueType>
    // struct select_type { using type = void; };

    // template<typename ValueType, typename CondType, typename ResType, typename... Args>
    // struct select_type
    // {
    //     using type = typename std::conditional_t<std::is_same_v<ValueType, CondType>, ResType, typename select_type<ValueType, Args...>::type>;
    // };
    //
    // template<typename ValueType, typename CondType, typename ResType, typename... Args>
    // using select_type_t = typename select_type<ValueType, CondType, ResType, Args...>::type;



}