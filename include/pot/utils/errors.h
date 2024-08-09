#pragma once

#include <stdexcept>

namespace pot::errors
{
    struct empty_error : public std::runtime_error
    {
        using runtime_error::runtime_error;
    };

    struct empty_result : public empty_error
    {
        using empty_error::empty_error;
    };

    struct task_failed : public empty_error
    {
        using empty_error::empty_error;
    };

    struct lazy_task_failed : public empty_error
    {
        using empty_error::empty_error;
    };

    struct iterrupted_error : public empty_error
    {
        using empty_error::empty_error;
    };

    struct big_task_promise_already_satisfied : public empty_error
    {
        using empty_error::empty_error;
    };

    struct big_shared_state_progress : public empty_error
    {
        using empty_error::empty_error;
    };
}
