#pragma once
#include <concepts>

namespace pot::traits::guards
{
    template<bool initial_value, typename BoolType>
        // requires std::convertible_to<BoolType, bool> && std::is_assignable_v<BoolType, bool>
    class guard_bool
    {
        // static_assert(std::convertible_to<BoolType, bool>, "BoolType must be convertible to bool");
        // static_assert(std::is_assignable_v<BoolType&, bool>, "BoolType must be assignable from bool");

        BoolType& m_variable;
    public:
        constexpr explicit guard_bool(BoolType& variable) : m_variable(variable) { m_variable = initial_value; }
        constexpr ~guard_bool() { m_variable = !initial_value; }
    };

    template<bool initial_value, typename BoolType>
    guard_bool(BoolType&) -> guard_bool<initial_value, BoolType>;

    template<bool initial_value, typename BoolType>
    constexpr auto make_guard_bool(BoolType& variable)
    {
        return guard_bool<initial_value, BoolType>(variable);
    }

    // template<bool initial_value, typename BoolType>
    // guard_bool(BoolType&, std::integral_constant<bool, initial_value>) -> guard_bool<initial_value, BoolType>;
}