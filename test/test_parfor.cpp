#include <catch2/catch_test_macros.hpp>

#include "pot/experimental/parfor.h"
#include "pot/utils/time_it.h"

TEST_CASE("pot::experimental::parfor")
{
    constexpr auto vec_size = 100000;
    constexpr auto experiment_count = 5;

    pot::experimental::thread_pool::thread_pool_fpe<true> pool;

    std::vector<double> vec_a(vec_size);
    std::vector<double> vec_b(vec_size);
    std::vector<double> vec_c(vec_size);

    std::ranges::fill(vec_a, 1.0);
    std::ranges::fill(vec_b, 2.0);
    auto clear_c = [&vec_c] { std::ranges::fill(vec_c, 0.0); };

    clear_c();

    const auto dur_for = pot::utils::time_it<std::chrono::nanoseconds>(experiment_count, clear_c, [&]
        {
            for (auto i = 0; i < vec_size; ++i)
            {
                vec_c[i] = vec_a[i] + vec_b[i];
            }
        });

    std::ranges::fill(vec_c, 0.0);

    const auto dur_parfor = pot::utils::time_it<std::chrono::nanoseconds>(experiment_count, clear_c, [&]
        {
            pot::experimental::parfor(pool, 0, vec_size, [&](const int i)
                {
                    vec_c[i] = vec_a[i] + vec_b[i];
                });
        });


    REQUIRE(dur_for > dur_parfor);

}