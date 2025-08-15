#pragma once

#include <immintrin.h>
#include <cinttypes>
#include <utility>

#include "pot/traits/compare.h"

namespace pot::simd
{
    template<typename scalar_type>
    concept simdable = (pot::traits::compare::is_one_of_type<scalar_type,
        int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double>);

    enum class SIMDType
    {
        SSE    = 128,
        AVX    = 256,
        AVX512 = 512,
    };

    template<simdable scalar_type, SIMDType simd_type>
    class simd_forced;
    template<simdable scalar_type, size_t scalar_count>
    class simd_auto;

    template<typename scalar_type>
    constexpr bool is_simd_forced = false;
    template<simdable scalar_type, SIMDType simd_type>
    constexpr bool is_simd_forced<simd_forced<scalar_type, simd_type>> = true;
    template<typename scalar_type>
    constexpr bool is_simd_auto = false;
    template<simdable scalar_type, size_t scalar_count>
    constexpr bool is_simd_auto<simd_auto<scalar_type, scalar_count>> = true;

    namespace details
    {
        constexpr auto bitness(SIMDType simd_type) { return std::to_underlying(simd_type); }
        constexpr auto byteness(SIMDType simd_type) { return bitness(simd_type) / 8; }

        template<simdable scalar_type, SIMDType simd_type>
        struct simd_traits;

        template<simdable scalar_type>
        struct simd_traits<scalar_type, SIMDType::SSE>
        {
            static constexpr size_t scalar_count = byteness(SIMDType::SSE) / sizeof(scalar_type);
            using vector_type = std::conditional_t<std::is_same_v<scalar_type, float>, __m128,
                std::conditional_t<std::is_same_v<scalar_type, double>, __m128d, __m128i>>;

            static constexpr auto mask()
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return 0xffff;
                if constexpr (std::is_same_v<vector_type, __m128d>) return 0x3;
                if constexpr (std::is_same_v<vector_type, __m128i>) return (1ULL << scalar_count) - 1;
            }

            static decltype(auto) get(const vector_type& a, size_t index)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return (static_cast<scalar_type*>(&a))[index];
                if constexpr (std::is_same_v<vector_type, __m128d>) return (static_cast<scalar_type*>(&a))[index];
                if constexpr (std::is_same_v<vector_type, __m128i>) return (static_cast<scalar_type*>(&a))[index];
            }

            static auto set1(scalar_type value)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_set1_ps(value);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_set1_pd(value);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_set1_epi32(static_cast<int>(value));
            }

            static auto store(scalar_type* ptr, vector_type value)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_store_ps(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_store_pd(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_store_si128(reinterpret_cast<__m128i*>(ptr), value);
            }
            static auto storeu(scalar_type* ptr, vector_type value)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_storeu_ps(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_storeu_pd(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr), value);
            }

            static auto load(const scalar_type* ptr)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_load_ps(ptr);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_load_pd(ptr);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_load_si128(reinterpret_cast<const __m128i*>(ptr));
            }

            static auto loadu(const scalar_type* ptr)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_loadu_ps(ptr);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_loadu_pd(ptr);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
            }

            static auto max(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_max_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_max_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_max_epi32(a, b);
            }

            // need tests
            static scalar_type max_scalar(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >)
                {
                    __m128 max1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1));
                    __m128 max2 = _mm_max_ps(a, max1);

                    __m128 max3 = _mm_shuffle_ps(max2, max2, _MM_SHUFFLE(1, 0, 3, 2));
                    __m128 max4 = _mm_max_ps(max2, max3);

                    return _mm_cvtss_f32(max4);
                }
                if constexpr (std::is_same_v<vector_type, __m128d>)
                {
                    __m128d max1 = _mm_shuffle_pd(a, a, _MM_SHUFFLE2(0, 1));
                    __m128d max2 = _mm_max_pd(a, max1);

                    return _mm_cvtsd_f64(max2);
                }
                if constexpr (std::is_same_v<vector_type, __m128i>)
                {
                    __m128i max1 = _mm_shuffle_epi32(a, _MM_SHUFFLE(2, 3, 0, 1));
                    __m128i max2 = _mm_max_epi32(a, max1);

                    __m128i max3 = _mm_shuffle_epi32(max2, _MM_SHUFFLE(1, 0, 3, 2));
                    __m128i max4 = _mm_max_epi32(max2, max3);

                    return static_cast<scalar_type>(_mm_cvtsi128_si32(max4));
                }
            }

            static auto min(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_min_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_min_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_min_epi32(a, b);
            }

            // need tests
            static scalar_type min(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >)
                {
                    __m128 min1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1));
                    __m128 min2 = _mm_min_ps(a, min1);

                    __m128 min3 = _mm_shuffle_ps(min2, min2, _MM_SHUFFLE(1, 0, 3, 2));
                    __m128 min4 = _mm_min_ps(min2, min3);

                    return _mm_cvtss_f32(min4);
                }
                if constexpr (std::is_same_v<vector_type, __m128d>)
                {
                    __m128d min1 = _mm_shuffle_pd(a, a, _MM_SHUFFLE2(0, 1));
                    __m128d min2 = _mm_min_pd(a, min1);

                    return _mm_cvtsd_f64(min2);
                }
                if constexpr (std::is_same_v<vector_type, __m128i>)
                {
                    __m128i min1 = _mm_shuffle_epi32(a, _MM_SHUFFLE(2, 3, 0, 1));
                    __m128i min2 = _mm_min_epi32(a, min1);

                    __m128i min3 = _mm_shuffle_epi32(min2, _MM_SHUFFLE(1, 0, 3, 2));
                    __m128i min4 = _mm_min_epi32(min2, min3);

                    return static_cast<scalar_type>(_mm_cvtsi128_si32(min4));
                }
            }

            static auto abs(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_and_ps(a, _mm_set1_ps(-0.f));
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_and_pd(a, _mm_set1_pd(-0.));
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_and_si128(a, _mm_set1_epi32(-0));
            }

            static auto sqrt(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_sqrt_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_sqrt_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_sqrt_epi32(a);
            }

            static auto sqr(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_mul_ps(a, a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_mul_pd(a, a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_mul_epi32(a, a);
            }

            static auto sum(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_hadd_ps(a, a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_hadd_pd(a, a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_hadd_epi32(a, a);
            }

            static auto prod(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_mul_ps(a, a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_mul_pd(a, a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_mul_epi32(a, a);
            }

            static auto exp(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_exp_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_exp_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_exp_epi32(a);
            }

            static auto log(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_log_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_log_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_log_epi32(a);
            }

            static auto log2(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_log2_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_log2_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_log2_epi32(a);
            }

            static auto log10(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_log10_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_log10_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_log10_epi32(a);
            }

            static auto sin(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_sin_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_sin_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_sin_epi32(a);
            }

            static auto cos(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_cos_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_cos_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_cos_epi32(a);
            }

            static auto tan(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_tan_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_tan_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_tan_epi32(a);
            }

            static auto asin(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_asin_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_asin_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_asin_epi32(a);
            }

            static auto acos(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_acos_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_acos_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_acos_epi32(a);
            }

            static auto atan(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_atan_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_atan_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_atan_epi32(a);
            }

            static auto sinh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_sinh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_sinh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_sinh_epi32(a);
            }

            static auto cosh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_cosh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_cosh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_cosh_epi32(a);
            }

            static auto tanh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_tanh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_tanh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_tanh_epi32(a);
            }

            static auto asinh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_asinh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_asinh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_asinh_epi32(a);
            }

            static auto acosh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_acosh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_acosh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_acosh_epi32(a);
            }

            static auto atanh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_atanh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_atanh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_atanh_epi32(a);
            }

            static auto ceil(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_ceil_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_ceil_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_ceil_epi32(a);
            }

            static auto floor(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_floor_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_floor_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_floor_epi32(a);
            }

            static auto trunc(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_trunc_ps(a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_trunc_pd(a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_trunc_epi32(a);
            }

            static auto round(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_round_pd(a, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_round_epi32(a, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
            }

            // operator+
            static auto add(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_add_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_add_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_add_epi32(a, b);
            }

            // operator-
            static auto sub(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_sub_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_sub_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_sub_epi32(a, b);
            }

            // operator*
            static auto mul(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_mul_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_mul_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_mul_epi32(a, b);
            }

            // operator/
            static auto div(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_div_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_div_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_div_epi32(a, b);
            }

            // operator%
            static auto mod(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_mod_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_mod_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_mod_epi32(a, b);
            }

            // operator&
            static auto and_(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_and_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_and_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_and_si128(a, b);
            }

            // operator|
            static auto or_(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_or_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_or_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_or_si128(a, b);
            }

            // operator^
            static auto xor_(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_xor_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_xor_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_xor_si128(a, b);
            }

            // operator <<
            static auto shl(const vector_type& a, int b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_slli_epi32(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_slli_epi64(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_slli_epi128(a, b);
            }
            // operator >>
            static auto shr(const vector_type& a, int b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_srli_epi32(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_srli_epi64(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_srli_epi128(a, b);
            }

            // operator~
            static auto not_(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_xor_ps(a, _mm_set1_ps(-0.f));
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_xor_pd(a, _mm_set1_pd(-0.));
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_xor_si128(a, _mm_set1_epi32(-0));
            }

            // operator-
            static auto neg(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_sub_ps(_mm_setzero_ps(), a);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_sub_pd(_mm_setzero_pd(), a);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_sub_epi32(_mm_setzero_si128(), a);
            }

            // operator+
            static auto pos(const vector_type& a)
            {
                return a;
            }

            // operator ==
            static auto cmpeq(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_cmpeq_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_cmpeq_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_cmpeq_epi32(a, b);
            }

            static bool cmpeq_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmpeq(a, b);
                return _mm_test_all_ones(_mm_castps_si128(temp));
            }


            // operator !=
            static auto cmpneq(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_cmpneq_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_cmpneq_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_cmpneq_epi32(a, b);
            }

            static bool cmpneq_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmpneq(a, b);
                return _mm_test_all_ones(_mm_castps_si128(temp));
            }


            // operator <
            static auto cmplt(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_cmplt_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_cmplt_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_cmplt_epi32(a, b);
            }
            static bool cmplt_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmplt(a, b);
                return _mm_test_all_ones(_mm_castps_si128(temp));
            }


            // operator <=
            static auto cmple(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_cmple_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_cmple_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_cmple_epi32(a, b);
            }
            static bool cmple_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmple(a, b);
                return _mm_test_all_ones(_mm_castps_si128(temp));
            }



            // operator >
            static auto cmpgt(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_cmpgt_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_cmpgt_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_cmpgt_epi32(a, b);
            }
            static bool cmpgt_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmpgt(a, b);
                return _mm_test_all_ones(_mm_castps_si128(temp));
            }

            // operator >=
            static auto cmpge(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m128 >) return _mm_cmpge_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m128d>) return _mm_cmpge_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m128i>) return _mm_cmpge_epi32(a, b);
            }
            static bool cmpge_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmpge(a, b);
                return _mm_test_all_ones(_mm_castps_si128(temp));
            }


        };

        // Специализация для AVX
        template<simdable scalar_type>
        struct simd_traits<scalar_type, SIMDType::AVX>
        {
            static constexpr size_t scalar_count = byteness(SIMDType::AVX) / sizeof(scalar_type);
            using vector_type = std::conditional_t<std::is_same_v<scalar_type, float>, __m256,
                std::conditional_t<std::is_same_v<scalar_type, double>, __m256d,
                __m256i>>;

            static constexpr auto mask()
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return 0xffffffff;
                if constexpr (std::is_same_v<vector_type, __m256d>) return 0xf;
                if constexpr (std::is_same_v<vector_type, __m256i>) return (1ULL << scalar_count) - 1;
            }

            template<int Index>
            static inline int extract_epi32(__m256i a) {
                static_assert(0 <= Index && Index < 8, "lane out of range");
                return _mm256_extract_epi32(a, Index);
            }

            static decltype(auto) get(const vector_type& a, size_t index)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_extract_ps(a, index);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_extract_pd(a, index);
                if constexpr (std::is_same_v<vector_type, __m256i>) return extract_epi32(a, index); // временно 
            }

            static auto set1(scalar_type value)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_set1_ps(value);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_set1_pd(value);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_set1_epi32(static_cast<int>(value));
            }

            static auto store(scalar_type* ptr, vector_type value)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_store_ps(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_store_pd(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_store_si256(reinterpret_cast<__m256i*>(ptr), value);
            }

            static auto storeu(scalar_type* ptr, vector_type value)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_storeu_ps(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_storeu_pd(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_storeu_si256(reinterpret_cast<__m256i*>(ptr), value);
            }

            static auto load(const scalar_type* ptr)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_load_ps(ptr);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_load_pd(ptr);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_load_si256(reinterpret_cast<const __m256i*>(ptr));
            }

            static auto loadu(const scalar_type* ptr)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_loadu_ps(ptr);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_loadu_pd(ptr);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
            }

            static auto max(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_max_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_max_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_max_epi32(a, b);
            }

            // need tests
            static scalar_type max_scalar(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >)
                {
                    auto temp = _mm256_max_ps(a, _mm256_permute2f128_ps(a, a, 1));
                    return _mm_cvtss_f32(_mm_max_ps(_mm_movehl_ps(temp, temp), temp));
                }
                if constexpr (std::is_same_v<vector_type, __m256d>)
                {
                    auto temp = _mm256_max_pd(a, _mm256_permute2f128_pd(a, a, 1));
                    return _mm_cvtsd_f64(_mm_max_pd(_mm_movehl_pd(temp, temp), temp));
                }
                if constexpr (std::is_same_v<vector_type, __m256i>)
                {
                    auto temp = _mm256_max_epi32(a, _mm256_permute2f128_si256(a, a, 1));
                    return _mm_cvtsi128_si32(_mm_max_epi32(_mm_movehl_epi32(temp, temp), temp));
                }
            }

            static auto min(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_min_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_min_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_min_epi32(a, b);
            }

            // need tests
            static scalar_type min(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >)
                {
                    auto temp = _mm256_min_ps(a, _mm256_permute2f128_ps(a, a, 1));
                    return _mm_cvtss_f32(_mm_min_ps(_mm_movehl_ps(temp, temp), temp));
                }
                if constexpr (std::is_same_v<vector_type, __m256d>)
                {
                    auto temp = _mm256_min_pd(a, _mm256_permute2f128_pd(a, a, 1));
                    return _mm_cvtsd_f64(_mm_min_pd(_mm_movehl_pd(temp, temp), temp));
                }
                if constexpr (std::is_same_v<vector_type, __m256i>)
                {
                    auto temp = _mm256_min_epi32(a, _mm256_permute2f128_si256(a, a, 1));
                    return _mm_cvtsi128_si32(_mm_min_epi32(_mm_movehl_epi32(temp, temp), temp));
                }
            }

            static auto abs(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_abs_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_abs_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_abs_epi32(a);
            }

            static auto sqrt(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_sqrt_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_sqrt_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_sqrt_epi32(a);
            }

            static auto sqr(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_mul_ps(a, a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_mul_pd(a, a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_mul_epi32(a, a);
            }

            static auto sum(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_hadd_ps(a, a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_hadd_pd(a, a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_hadd_epi32(a, a);
            }

            static auto prod(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_mul_ps(a, a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_mul_pd(a, a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_mul_epi32(a, a);
            }

            static auto exp(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_exp_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_exp_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_exp_epi32(a);
            }

            static auto log(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_log_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_log_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_log_epi32(a);
            }

            static auto log2(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_log2_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_log2_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_log2_epi32(a);
            }

            static auto log10(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_log10_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_log10_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_log10_epi32(a);
            }

            static auto sin(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_sin_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_sin_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_sin_epi32(a);
            }

            static auto cos(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_cos_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_cos_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_cos_epi32(a);
            }

            static auto tan(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_tan_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_tan_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_tan_epi32(a);
            }

            static auto asin(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_asin_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_asin_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_asin_epi32(a);
            }

            static auto acos(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_acos_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_acos_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_acos_epi32(a);
            }

            static auto atan(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_atan_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_atan_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_atan_epi32(a);
            }

            static auto sinh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_sinh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_sinh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_sinh_epi32(a);
            }

            static auto cosh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_cosh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_cosh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_cosh_epi32(a);
            }

            static auto tanh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_tanh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_tanh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_tanh_epi32(a);
            }

            static auto asinh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_asinh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_asinh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_asinh_epi32(a);
            }

            static auto acosh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_acosh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_acosh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_acosh_epi32(a);
            }

            static auto atanh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_atanh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_atanh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_atanh_epi32(a);
            }

            static auto ceil(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_ceil_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_ceil_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_ceil_epi32(a);
            }

            static auto floor(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_floor_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_floor_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_floor_epi32(a);
            }

            static auto trunc(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_trunc_ps(a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_trunc_pd(a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_trunc_epi32(a);
            }

            static auto round(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_round_pd(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_round_epi32(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            }

            // operator+
            static auto add(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_add_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_add_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_add_epi32(a, b);
            }

            // operator-
            static auto sub(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_sub_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_sub_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_sub_epi32(a, b);
            }

            // operator*
            static auto mul(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_mul_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_mul_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_mul_epi32(a, b);
            }

            // operator/
            static auto div(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_div_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_div_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_div_epi32(a, b);
            }

            // operator%
            static auto mod(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_rem_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_rem_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_rem_epi32(a, b);
            }

            // operator&
            static auto and_(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_and_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_and_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_and_si256(a, b);
            }

            // operator|
            static auto or_(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_or_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_or_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_or_si256(a, b);
            }

            // operator^
            static auto xor_(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_xor_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_xor_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_xor_si256(a, b);
            }

            // operator<<
            static auto shl(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_slli_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_slli_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_slli_epi32(a, b);
            }

            // operator>>
            static auto shr(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_srli_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_srli_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_srli_epi32(a, b);
            }

            // operator~
            static auto not_(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_xor_ps(a, _mm256_set1_ps(-0.f));
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_xor_pd(a, _mm256_set1_pd(-0.));
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_xor_si256(a, _mm256_set1_epi32(-1));
            }

            // operator-
            static auto neg(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_sub_ps(_mm256_setzero_ps(), a);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_sub_pd(_mm256_setzero_pd(), a);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_sub_epi32(_mm256_setzero_si256(), a);
            }

            // operator+
            static auto pos(const vector_type& a)
            {
                return a;
            }

            // operator==
            static auto cmpeq(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_cmp_ps(a, b, _CMP_EQ_OQ);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_cmp_pd(a, b, _CMP_EQ_OQ);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_cmp_epi32(a, b, _CMP_EQ_OQ);
            }

            static bool cmpeq_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmpeq(a, b);
                return _mm256_test_all_ones(_mm256_castps_si256(temp));
            }

            // operator!=
            static auto cmpneq(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_cmp_ps(a, b, _CMP_NEQ_OQ);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_cmp_pd(a, b, _CMP_NEQ_OQ);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_cmp_epi32(a, b, _CMP_NEQ_OQ);
            }

            static bool cmpneq_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmpneq(a, b);
                return _mm256_test_all_ones(_mm256_castps_si256(temp));
            }

            // operator<
            static auto cmplt(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_cmp_ps(a, b, _CMP_LT_OQ);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_cmp_pd(a, b, _CMP_LT_OQ);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_cmp_epi32(a, b, _CMP_LT_OQ);
            }
            static bool cmplt_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmplt(a, b);
                return _mm256_test_all_ones(_mm256_castps_si256(temp));
            }

            // operator<=
            static auto cmple(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_cmp_ps(a, b, _CMP_LE_OQ);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_cmp_pd(a, b, _CMP_LE_OQ);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_cmp_epi32(a, b, _CMP_LE_OQ);
            }
            static bool cmple_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmple(a, b);
                return _mm256_test_all_ones(_mm256_castps_si256(temp));
            }

            // operator>
            static auto cmpgt(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_cmp_ps(a, b, _CMP_GT_OQ);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_cmp_pd(a, b, _CMP_GT_OQ);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_cmp_epi32(a, b, _CMP_GT_OQ);
            }
            static bool cmpgt_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmpgt(a, b);
                return _mm256_test_all_ones(_mm256_castps_si256(temp));
            }

            // operator>=
            static auto cmpge(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m256 >) return _mm256_cmp_ps(a, b, _CMP_GE_OQ);
                if constexpr (std::is_same_v<vector_type, __m256d>) return _mm256_cmp_pd(a, b, _CMP_GE_OQ);
                if constexpr (std::is_same_v<vector_type, __m256i>) return _mm256_cmp_epi32(a, b, _CMP_GE_OQ);
            }
            static bool cmpge_bool(const vector_type& a, const vector_type& b)
            {
                auto temp = cmpge(a, b);
                return _mm256_test_all_ones(_mm256_castps_si256(temp));
            }

        };

        // Специализация для AVX-512
        template<simdable scalar_type>
        struct simd_traits<scalar_type, SIMDType::AVX512>
        {
            static constexpr size_t scalar_count = byteness(SIMDType::AVX512) / sizeof(scalar_type);
            using vector_type = std::conditional_t<std::is_same_v<scalar_type, float>, __m512,
                std::conditional_t<std::is_same_v<scalar_type, double>, __m512d,
                __m512i>>;

            static constexpr auto mask()
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return 0xffffffffffffffff;
                if constexpr (std::is_same_v<vector_type, __m512d>) return 0xff;
                if constexpr (std::is_same_v<vector_type, __m512i>) return (1ULL << scalar_count) - 1;
            }

            static decltype(auto) get(const vector_type& value, size_t index)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return value[index];
                if constexpr (std::is_same_v<vector_type, __m512d>) return value[index];
                if constexpr (std::is_same_v<vector_type, __m512i>) return value[index];
            }

            static auto set1(scalar_type value)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_set1_ps(value);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_set1_pd(value);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_set1_epi32(static_cast<int>(value));
            }

            static auto store(scalar_type* ptr, vector_type value)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_store_ps(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_store_pd(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_store_si512(reinterpret_cast<__m512i*>(ptr), value);
            }

            static auto storeu(scalar_type* ptr, vector_type value)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_storeu_ps(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_storeu_pd(ptr, value);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_storeu_si512(reinterpret_cast<__m512i*>(ptr), value);
            }

            static auto load(const scalar_type* ptr)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_load_ps(ptr);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_load_pd(ptr);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_load_si512(reinterpret_cast<const __m512i*>(ptr));
            }

            static auto loadu(const scalar_type* ptr)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_loadu_ps(ptr);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_loadu_pd(ptr);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
            }

            static auto max(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_max_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_max_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_max_epi32(a, b);
            }

            // need tests
            static scalar_type max_scalar(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_reduce_max_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_reduce_max_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_reduce_max_epi32(a);
            }

            static auto min(const vector_type& a, const vector_type& b)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_min_ps(a, b);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_min_pd(a, b);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_min_epi32(a, b);
            }

            static scalar_type min_scalar(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_reduce_min_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_reduce_min_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_reduce_min_epi32(a);
            }

            static auto abs(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_abs_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_abs_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_abs_epi32(a);
            }

            static auto sqrt(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_sqrt_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_sqrt_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_sqrt_epi32(a);
            }

            static auto sqr(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_mul_ps(a, a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_mul_pd(a, a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_mul_epi32(a, a);
            }

            static auto sum(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_reduce_add_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_reduce_add_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_reduce_add_epi32(a);
            }

            static auto prod(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_reduce_mul_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_reduce_mul_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_reduce_mul_epi32(a);
            }

            static auto exp(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_exp_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_exp_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_exp_epi32(a);
            }

            static auto log(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_log_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_log_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_log_epi32(a);
            }

            static auto log2(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_log2_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_log2_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_log2_epi32(a);
            }

            static auto log10(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_log10_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_log10_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_log10_epi32(a);
            }

            static auto sin(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_sin_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_sin_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_sin_epi32(a);
            }

            static auto cos(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_cos_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_cos_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_cos_epi32(a);
            }

            static auto tan(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_tan_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_tan_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_tan_epi32(a);
            }

            static auto asin(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_asin_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_asin_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_asin_epi32(a);
            }

            static auto acos(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_acos_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_acos_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_acos_epi32(a);
            }

            static auto atan(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_atan_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_atan_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_atan_epi32(a);
            }

            static auto sinh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_sinh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_sinh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_sinh_epi32(a);
            }

            static auto cosh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_cosh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_cosh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_cosh_epi32(a);
            }

            static auto tanh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_tanh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_tanh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_tanh_epi32(a);
            }

            static auto asinh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_asinh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_asinh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_asinh_epi32(a);
            }

            static auto acosh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_acosh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_acosh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_acosh_epi32(a);
            }

            static auto atanh(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_atanh_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_atanh_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_atanh_epi32(a);
            }

            static auto ceil(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_ceil_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_ceil_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_ceil_epi32(a);
            }

            static auto floor(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_floor_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_floor_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_floor_epi32(a);
            }

            static auto trunc(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_trunc_ps(a);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_trunc_pd(a);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_trunc_epi32(a);
            }

            static auto round(const vector_type& a)
            {
                if constexpr (std::is_same_v<vector_type, __m512 >) return _mm512_round_ps(a, _MM_FROUND_TO_NEAREST_INT);
                if constexpr (std::is_same_v<vector_type, __m512d>) return _mm512_round_pd(a, _MM_FROUND_TO_NEAREST_INT);
                if constexpr (std::is_same_v<vector_type, __m512i>) return _mm512_round_epi32(a, _MM_FROUND_TO_NEAREST_INT);
            }

        };



    }
} // namespace pot::simd
