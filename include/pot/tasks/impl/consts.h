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

    // big_task

    inline const char* big_task_interrupted_error_msg = "pot::big_task::get() - task was interrupted.";

    // big_task_promise 

    inline const char* big_task_promise_already_satisfied_set_value_error_msg = "pot::big_task_promise::set_value() - promise already satisfied.";

    inline const char* big_task_promise_already_satisfied_set_exception_error_msg = "pot::big_task_promise::set_exception() - promise already satisfied.";

    // big_shared_state

    inline const char* big_shared_state_progress_out_of_range_error_msg = "pot::tasks::details::big_shared_state::set_progress() - Progress must be between 0 and 100.";

    inline const char* big_shared_state_set_progress_interrupted_error_msg = "pot::tasks:details::big_shared_state::set_progress() - Can not set progress (interrupted).";
    

    /* executors */

    // thread_pool_executor

}