#pragma once

#include <immintrin.h>

#include "pot/simd/simd_traits.h"

namespace pot::simd
{
    template<simdable scalar_type, SIMDType simd_type>
    class simd_forced
    {
        using trait = details::simd_traits<scalar_type, simd_type>;
        trait::vector_type m_value;
    public:
        simd_forced() = default;
        simd_forced(const simd_forced&) = default;
        simd_forced(simd_forced&&) = default;
        simd_forced& operator=(const simd_forced&) = default;
        simd_forced& operator=(simd_forced&&) = default;
        ~simd_forced() = default;

        simd_forced(const trait::vector_type& value) : m_value(value) {}
        simd_forced(trait::vector_type&& value) : m_value(std::move(value)) {}
        explicit simd_forced(scalar_type value) : m_value(trait::set1(value)) {}

        template<typename... Args>
            requires((sizeof...(Args) == trait::scalar_count) && (std::is_same_v<scalar_type, Args> && ...))
        explicit simd_forced(Args... args) : m_value(trait::set(args...)) {}

        scalar_type* data() { return reinterpret_cast<scalar_type*>(&m_value); }

        void load(const scalar_type* ptr) { m_value = trait::load(ptr); }
        void loadu(const scalar_type* ptr) { m_value = trait::loadu(ptr); }

        void store(scalar_type* ptr) const { trait::store(ptr, m_value); }
        void storeu(scalar_type* ptr) const { trait::storeu(ptr, m_value); }

        static simd_forced zeros() { return simd_forced(scalar_type(0)); }
        static simd_forced ones()  { return simd_forced(scalar_type(1)); }

        scalar_type max  () const { return trait::max_scalar(m_value); }
        simd_forced max  (const simd_forced& other) const { return trait::max(m_value, other.m_value); }
        scalar_type min  () const { return trait::min_scalar(m_value); }
        simd_forced min  (const simd_forced& other) const { return trait::min(m_value, other.m_value); }
        simd_forced abs  () const { return trait::abs(m_value); }
        simd_forced sqrt () const { return trait::sqrt(m_value); }
        simd_forced sqr  () const { return trait::sqr(m_value); }
        scalar_type sum  () const { return trait::sum(m_value); }
        scalar_type prod () const { return trait::prod(m_value); }
        simd_forced exp  () const { return trait::exp(m_value); }
        simd_forced log  () const { return trait::log(m_value); }
        simd_forced log2 () const { return trait::log2(m_value); }
        simd_forced log10() const { return trait::log10(m_value); }
        simd_forced sin  () const { return trait::sin(m_value); }
        simd_forced cos  () const { return trait::cos(m_value); }
        simd_forced tan  () const { return trait::tan(m_value); }
        simd_forced asin () const { return trait::asin(m_value); }
        simd_forced acos () const { return trait::acos(m_value); }
        simd_forced atan () const { return trait::atan(m_value); }
        simd_forced sinh () const { return trait::sinh(m_value); }
        simd_forced cosh () const { return trait::cosh(m_value); }
        simd_forced tanh () const { return trait::tanh(m_value); }
        simd_forced asinh() const { return trait::asinh(m_value); }
        simd_forced acosh() const { return trait::acosh(m_value); }
        simd_forced atanh() const { return trait::atanh(m_value); }
        simd_forced ceil () const { return trait::ceil(m_value); }
        simd_forced floor() const { return trait::floor(m_value); }
        simd_forced trunc() const { return trait::trunc(m_value); }
        simd_forced round() const { return trait::round(m_value); }

        simd_forced operator+ (const simd_forced& rhs) const { return trait::add (m_value, rhs.m_value); }
        simd_forced operator- (const simd_forced& rhs) const { return trait::sub (m_value, rhs.m_value); }
        simd_forced operator* (const simd_forced& rhs) const { return trait::mul (m_value, rhs.m_value); }
        simd_forced operator/ (const simd_forced& rhs) const { return trait::div (m_value, rhs.m_value); }
        simd_forced operator% (const simd_forced& rhs) const { return trait::mod (m_value, rhs.m_value); }
        simd_forced operator& (const simd_forced& rhs) const { return trait::and_(m_value, rhs.m_value); }
        simd_forced operator| (const simd_forced& rhs) const { return trait::or_ (m_value, rhs.m_value); }
        simd_forced operator^ (const simd_forced& rhs) const { return trait::xor_(m_value, rhs.m_value); }
        simd_forced operator<<(const scalar_type  rhs) const { return trait::shl (m_value, static_cast<int>(rhs)); }
        simd_forced operator>>(const scalar_type  rhs) const { return trait::shr (m_value, static_cast<int>(rhs)); }

        bool operator==(const simd_forced& rhs) const { return trait::cmpeq_bool (m_value, rhs.m_value); }
        bool operator!=(const simd_forced& rhs) const { return trait::cmpneq_bool(m_value, rhs.m_value); }
        bool operator< (const simd_forced& rhs) const { return trait::cmplt_bool (m_value, rhs.m_value); }
        bool operator<=(const simd_forced& rhs) const { return trait::cmple_bool (m_value, rhs.m_value); }
        bool operator> (const simd_forced& rhs) const { return trait::cmpgt_bool (m_value, rhs.m_value); }
        bool operator>=(const simd_forced& rhs) const { return trait::cmpge_bool (m_value, rhs.m_value); }

        // simd_forced operator==(const simd_forced& rhs) const { return trait::cmpeq (m_value, rhs.m_value); }
        // simd_forced operator!=(const simd_forced& rhs) const { return trait::cmpneq(m_value, rhs.m_value); }
        // simd_forced operator< (const simd_forced& rhs) const { return trait::cmplt (m_value, rhs.m_value); }
        // simd_forced operator<=(const simd_forced& rhs) const { return trait::cmple (m_value, rhs.m_value); }
        // simd_forced operator> (const simd_forced& rhs) const { return trait::cmpgt (m_value, rhs.m_value); }
        // simd_forced operator>=(const simd_forced& rhs) const { return trait::cmpge (m_value, rhs.m_value); }

        simd_forced operator~() const { return trait::not_(m_value); }
        simd_forced operator-() const { return trait::neg(m_value); }
        simd_forced operator+() const { return m_value; }
        simd_forced operator++() { m_value = trait::add(m_value, trait::set1(1)); return m_value; }
        simd_forced operator--() { m_value = trait::sub(m_value, trait::set1(1)); return m_value; }
        simd_forced operator+=(const simd_forced& rhs) { m_value = trait::add(m_value, rhs.m_value); return m_value; }
        simd_forced operator-=(const simd_forced& rhs) { m_value = trait::sub(m_value, rhs.m_value); return m_value; }
        simd_forced operator*=(const simd_forced& rhs) { m_value = trait::mul(m_value, rhs.m_value); return m_value; }
        simd_forced operator/=(const simd_forced& rhs) { m_value = trait::div(m_value, rhs.m_value); return m_value; }
        simd_forced operator%=(const simd_forced& rhs) { m_value = trait::mod(m_value, rhs.m_value); return m_value; }
        simd_forced operator&=(const simd_forced& rhs) { m_value = trait::and_(m_value, rhs.m_value); return m_value; }
        simd_forced operator|=(const simd_forced& rhs) { m_value = trait::or_(m_value, rhs.m_value); return m_value; }
        simd_forced operator^=(const simd_forced& rhs) { m_value = trait::xor_(m_value, rhs.m_value); return m_value; }
        simd_forced operator<<=(const simd_forced& rhs) { m_value = trait::shl(m_value, rhs.m_value); return m_value; }
        simd_forced operator>>=(const simd_forced& rhs) { m_value = trait::shr(m_value, rhs.m_value); return m_value; }
        simd_forced operator++(int) { simd_forced tmp = m_value; m_value = trait::add(m_value, trait::set1(1)); return tmp; }
        simd_forced operator--(int) { simd_forced tmp = m_value; m_value = trait::sub(m_value, trait::set1(1)); return tmp; }

        scalar_type& operator[](size_t index) { return trait::get(m_value, index); }
        const scalar_type& operator[](size_t index) const { return trait::get(m_value, index); }

        scalar_type& operator()(size_t index) { return trait::get(m_value, index); }
        const scalar_type& operator()(size_t index) const { return trait::get(m_value, index); }

    };



} // namespace pot::simd

