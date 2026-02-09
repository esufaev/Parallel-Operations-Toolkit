#pragma once

#include <memory_resource>

namespace pot::memory
{

inline std::pmr::synchronized_pool_resource &get_coro_pool()
{
    static std::pmr::synchronized_pool_resource pool(std::pmr::new_delete_resource());
    return pool;
}

} // namespace pot::memory
