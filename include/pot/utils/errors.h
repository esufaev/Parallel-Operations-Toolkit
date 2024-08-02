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
}
