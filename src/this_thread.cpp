#include "pot/this_thread.h"

void pot::details::this_thread::init_thread_variables(const int64_t local_id, const std::weak_ptr<executor>& owner_executor)
{
    static std::atomic<int64_t> thread_counter{ 0 };
    tl_local_id = local_id;
    tl_global_id = thread_counter++;

    tl_owner_executor = owner_executor;
}

int64_t pot::this_thread::system_id()
{
    return static_cast<int64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

int64_t pot::this_thread::local_id()
{
    return details::this_thread::tl_local_id;
}

int64_t pot::this_thread::global_id()
{
    return details::this_thread::tl_global_id;
}

void pot::this_thread::yield()
{
    std::this_thread::yield();
}
