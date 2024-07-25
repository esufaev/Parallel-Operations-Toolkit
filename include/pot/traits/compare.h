#pragma once


namespace pot::traits::compare
{

    template<typename T, typename... Ts>
    [[nodiscard]] constexpr bool is_one_of(T&& value, Ts&&... values)
    {
        return ((value == values) || ...);
    }

}