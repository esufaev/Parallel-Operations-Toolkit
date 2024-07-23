#pragma once

#include <thread>
#include <chrono>

#include "executor.h"
#include "pot/executors/thread_pool_executor.h"

namespace pot::this_thread
{
    inline std::atomic<int64_t> thread_counter{0};

    thread_local int64_t id = static_cast<int64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    thread_local int64_t local_id = thread_counter++;

    int64_t get_id() { return id; }

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

    void yield()
    {
        std::this_thread::yield();
    }

    thread_local std::weak_ptr<pot::executor> executor;
}