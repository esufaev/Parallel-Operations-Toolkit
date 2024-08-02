#pragma once 

namespace pot::details::consts
{
    /* tasks */ 

    // task
    inline const char* task_get_error_msg = "pot::task::get() - result is empty.";

    inline const char* task_wait_error_msg = "pot::task::wait() - result is empty.";

    inline const char* task_wait_for_error_msg = "pot::task::wait_for() - result is empty.";

    inline const char* task_wait_until_error_msg = "pot::task::wait_until() - result is empty.";

    // stack_task

    inline const char* shared_task_get_error_msg = "pot::shared_task::get() - result is empty.";

    inline const char* shared_task_wait_error_msg = "pot::shared_task::wait() - result is empty.";

    inline const char* shared_task_wait_for_error_msg = "pot::shared_task::wait_for() - result is empty.";

    inline const char* shared_task_wait_until_error_msg = "pot::shared_task::wait_until() - result is empty.";

    // lazy_task
    inline const char* lazy_task_get_error_msg = "pot::lazy_task::get() - result is empty.";

    inline const char* lazy_task_wait_error_msg = "pot::lazy_task::wait() - result is empty.";

    inline const char* lazy_task_wait_for_error_msg = "pot::lazy_task::wait_for() - result is empty.";

    inline const char* lazy_task_wait_until_error_msg = "pot::lazy_task::wait_until() - result is empty.";

    /* executors */

    // thread_pool_executor

}