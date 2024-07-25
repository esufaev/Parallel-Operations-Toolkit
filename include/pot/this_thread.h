#pragma once

#include <thread>
#include <chrono>

#include "pot/executor.h"


namespace pot::details::this_thread
{
    void init_thread_variables(int64_t local_id = 0, const std::weak_ptr<executor>& owner_executor = {});

    inline thread_local int64_t tl_local_id ; // Identifier inside a specific executor
    inline thread_local int64_t tl_global_id; // Global identifier across all executors
    inline thread_local std::weak_ptr<executor> tl_owner_executor;
}


namespace pot::this_thread
{
    [[nodiscard]] int64_t system_id();
    [[nodiscard]] int64_t  local_id();
    [[nodiscard]] int64_t global_id();

    void set_name(const std::string& name);

    template <typename Rep, typename Period>
    void sleep_for(const std::chrono::duration<Rep, Period> &duration)
    {
        std::this_thread::sleep_for(duration);
    }

    template <typename Clock, typename Duration>
    void sleep_until(const std::chrono::time_point<Clock, Duration> &time_point)
    {
        std::this_thread::sleep_until(time_point);
    }

    void yield();
}


