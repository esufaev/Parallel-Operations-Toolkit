#pragma once

#include "pot/simd/simd_traits.h"

namespace pot::simd
{
    template<simdable scalar_type, size_t scalar_count>
    class simd_auto
    {
        using align_bound = std::max(alignof(scalar_type), alignof(std::max_align_t))
        using vector_type = alignas(align_bound) scalar_type[scalar_count];
        vector_type m_value;

    public:
        constexpr simd_auto() = default;
        constexpr simd_auto(const simd_auto&) = default;
        constexpr simd_auto(simd_auto&&) = default;
        constexpr simd_auto& operator=(const simd_auto&) = default;
        constexpr simd_auto& operator=(simd_auto&&) = default;
        constexpr ~simd_auto() = default;

        constexpr simd_auto(scalar_type value)
        {
            for (size_t i = 0; i < scalar_count; ++i)
            {
                m_value[i] = value;
            }
        }

        template<typename... Args>
            requires((sizeof...(Args) == scalar_count) && (std::is_same_v<scalar_type, Args> && ...))
        explicit simd_auto(Args... args)
        {
            for(size_t i = 0; i < scalar_count; ++i)
            {
                m_value[i] = args[i];
            }
        }

        constexpr scalar_type* data() { return m_data; }

        constexpr void load(const scalar_type* ptr)
        {
            for (size_t i = 0; i < scalar_count; ++i)
            {
                m_data[i] = ptr[i];
            }
        }

        constexpr void store(scalar_type* ptr) const
        {
            for (size_t i = 0; i < scalar_count; ++i)
            {
                ptr[i] = m_data[i];
            }
        }

        static simd_auto zeros() const { return simd_auto(scalar_type(0)); }
        static simd_auto ones() const { return simd_auto(scalar_type(1)); }

        scalar_type max  () const;
        simd_auto   max  (const simd_auto& other) const;
        scalar_type min  () const;
        simd_auto   min  (const simd_auto& other) const;
        simd_auto   abs  () const;
        simd_auto   sqrt () const;
        simd_auto   sqr  () const;
        scalar_type sum  () const;
        scalar_type prod () const;
        simd_auto   exp  () const;
        simd_auto   log  () const;
        simd_auto   log2 () const;
        simd_auto   log10() const;
        simd_auto   sin  () const;
        simd_auto   cos  () const;
        simd_auto   tan  () const;
        simd_auto   asin () const;
        simd_auto   acos () const;
        simd_auto   atan () const;
        simd_auto   sinh () const;
        simd_auto   cosh () const;
        simd_auto   tanh () const;
        simd_auto   asinh() const;
        simd_auto   acosh() const;
        simd_auto   atanh() const;
        simd_auto   ceil () const;
        simd_auto   floor() const;
        simd_auto   trunc() const;
        simd_auto   round() const;

        simd_auto operator+ (const simd_auto&  rhs) const;
        simd_auto operator- (const simd_auto&  rhs) const;
        simd_auto operator* (const simd_auto&  rhs) const;
        simd_auto operator/ (const simd_auto&  rhs) const;
        simd_auto operator% (const simd_auto&  rhs) const;
        simd_auto operator& (const simd_auto&  rhs) const;
        simd_auto operator| (const simd_auto&  rhs) const;
        simd_auto operator^ (const simd_auto&  rhs) const;
        simd_auto operator<<(const scalar_type rhs) const;
        simd_auto operator>>(const scalar_type rhs) const;

        bool operator==(const simd_auto& rhs) const;
        bool operator!=(const simd_auto& rhs) const;
        bool operator< (const simd_auto& rhs) const;
        bool operator<=(const simd_auto& rhs) const;
        bool operator> (const simd_auto& rhs) const;
        bool operator>=(const simd_auto& rhs) const;

        simd_auto operator==(const simd_auto& rhs) const;
        simd_auto operator!=(const simd_auto& rhs) const;
        simd_auto operator< (const simd_auto& rhs) const;
        simd_auto operator<=(const simd_auto& rhs) const;
        simd_auto operator> (const simd_auto& rhs) const;
        simd_auto operator>=(const simd_auto& rhs) const;

        simd_auto operator~() const;
        simd_auto operator-() const;
        simd_auto operator+() const;
        simd_auto operator++();
        simd_auto operator--();
        simd_auto operator+= (const simd_auto& rhs);
        simd_auto operator-= (const simd_auto& rhs);
        simd_auto operator*= (const simd_auto& rhs);
        simd_auto operator/= (const simd_auto& rhs);
        simd_auto operator%= (const simd_auto& rhs);
        simd_auto operator&= (const simd_auto& rhs);
        simd_auto operator|= (const simd_auto& rhs);
        simd_auto operator^= (const simd_auto& rhs);
        simd_auto operator<<=(const simd_auto& rhs);
        simd_auto operator>>=(const simd_auto& rhs);
        simd_auto operator++ (int);
        simd_auto operator-- (int);

        scalar_type& operator[](size_t index);
        const scalar_type& operator[](size_t index) const;

        scalar_type& operator()(size_t index);
        const scalar_type& operator()(size_t index) const;



    };

    template<simdable scalar_type, size_t scalar_count>
    inline scalar_type simd_auto<scalar_type, scalar_count>::max() const
    {
        scalar_type max = m_value[0];
        for(size_t i = 1; i < scalar_count; ++i)
        {
            max = std::max(max, m_value[i]);
        }

        return max;
    }
    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::max(const simd_auto& other) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::max(m_value[i], other.m_value[i]);
        }

        return res;
    }
    template<simdable scalar_type, size_t scalar_count>
    inline scalar_type simd_auto<scalar_type, scalar_count>::min() const
    {
        scalar_type min = m_value[0];
        for(size_t i = 1; i < scalar_count; ++i)
        {
            min = std::min(min, m_value[i]);
        }

        return min;
    }
    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::min(const simd_auto& other) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::min(m_value[i], other.m_value[i]);
        }

        return res;
    }
    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::abs() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::abs(m_value[i]);
        }

        return res;
    }
    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::sqrt() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::sqrt(m_value[i]);
        }

        return res;
    }
    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::sqr() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] * m_value[i];
        }

        return res;
    }
    template<simdable scalar_type, size_t scalar_count>
    inline scalar_type simd_auto<scalar_type, scalar_count>::sum() const
    {
        scalar_type sum(0);
        for(size_t i = 0; i < scalar_count; ++i)
        {
            sum += m_value[i];
        }

        return sum;
    }
    template<simdable scalar_type, size_t scalar_count>
    inline scalar_type simd_auto<scalar_type, scalar_count>::prod() const
    {
        scalar_type prod(1);
        for(size_t i = 0; i < scalar_count; ++i)
        {
            prod *= m_value[i];
        }

        return prod;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::exp() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::exp(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::log() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::log(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::log2() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::log2(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::log10() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::log10(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::sin() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::sin(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::cos() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::cos(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::tan() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::tan(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::asin() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::asin(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::acos() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::acos(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::atan() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::atan(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::sinh() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::sinh(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::cosh() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::cosh(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::tanh() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::tanh(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::asinh() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::asinh(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::acosh() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::acosh(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::atanh() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::atanh(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::ceil() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::ceil(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::floor() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::floor(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::trunc() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::trunc(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::round() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = std::round(m_value[i]);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator+(const simd_auto& rhs) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] + rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator-(const simd_auto& rhs) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] - rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator*(const simd_auto& rhs) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] * rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator/(const simd_auto& rhs) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] / rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator%(const simd_auto& rhs) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] % rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator&(const simd_auto& rhs) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] & rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator|(const simd_auto& rhs) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] | rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator^(const simd_auto& rhs) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] ^ rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator<<(const scalar_type rhs) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] << static_cast<int>(rhs);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator>>(const scalar_type rhs) const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i] >> static_cast<int>(rhs);
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline bool simd_auto<scalar_type, scalar_count>::operator==(const simd_auto& rhs) const
    {
        for(size_t i = 0; i < scalar_count; ++i)
        {
            if(m_value[i] != rhs.m_value[i])
            {
                return false;
            }
        }

        return true;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline bool simd_auto<scalar_type, scalar_count>::operator!=(const simd_auto& rhs) const
    {
        return !(*this == rhs);
    }

    template<simdable scalar_type, size_t scalar_count>
    inline bool simd_auto<scalar_type, scalar_count>::operator<(const simd_auto& rhs) const
    {
        for(size_t i = 0; i < scalar_count; ++i)
        {
            if(m_value[i] >= rhs.m_value[i])
            {
                return false;
            }
        }

        return true;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline bool simd_auto<scalar_type, scalar_count>::operator<=(const simd_auto& rhs) const
    {
        for(size_t i = 0; i < scalar_count; ++i)
        {
            if(m_value[i] > rhs.m_value[i])
            {
                return false;
            }
        }

        return true;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline bool simd_auto<scalar_type, scalar_count>::operator>(const simd_auto& rhs) const
    {
        for(size_t i = 0; i < scalar_count; ++i)
        {
            if(m_value[i] <= rhs.m_value[i])
            {
                return false;
            }
        }

        return true;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline bool simd_auto<scalar_type, scalar_count>::operator>=(const simd_auto& rhs) const
    {
        for(size_t i = 0; i < scalar_count; ++i)
        {
            if(m_value[i] < rhs.m_value[i])
            {
                return false;
            }
        }

        return true;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator~() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = ~m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator-() const
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = -m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator+() const
    {
        return *this;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator++()
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = ++m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator--()
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = --m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator+=(const simd_auto& rhs)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] += rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator-=(const simd_auto& rhs)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] -= rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator*=(const simd_auto& rhs)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] *= rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator/=(const simd_auto& rhs)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] /= rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator%=(const simd_auto& rhs)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] %= rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator&=(const simd_auto& rhs)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] &= rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator|=(const simd_auto& rhs)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] |= rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator^=(const simd_auto& rhs)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] ^= rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator<<=(const simd_auto& rhs)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] <<= rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator>>=(const simd_auto& rhs)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] >>= rhs.m_value[i];
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator++(int)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i]++;
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline simd_auto<scalar_type, scalar_count> simd_auto<scalar_type, scalar_count>::operator--(int)
    {
        simd_auto res;
        for(size_t i = 0; i < scalar_count; ++i)
        {
            res.m_value[i] = m_value[i]--;
        }

        return res;
    }

    template<simdable scalar_type, size_t scalar_count>
    inline scalar_type& simd_auto<scalar_type, scalar_count>::operator[](size_t index)
    {
        assert(index < scalar_count);
        return m_value[index];
    }

    template<simdable scalar_type, size_t scalar_count>
    inline const scalar_type& simd_auto<scalar_type, scalar_count>::operator[](size_t index) const
    {
        assert(index < scalar_count);
        return m_value[index];
    }

    template<simdable scalar_type, size_t scalar_count>
    inline scalar_type& simd_auto<scalar_type, scalar_count>::operator()(size_t index)
    {
        assert(index < scalar_count);
        return m_value[index];
    }

    template<simdable scalar_type, size_t scalar_count>
    inline const scalar_type& simd_auto<scalar_type, scalar_count>::operator()(size_t index) const
    {
        assert(index < scalar_count);
        return m_value[index];
    }

} // namespace pot::simd

