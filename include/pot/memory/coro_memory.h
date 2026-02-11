#pragma once

#include <atomic>
#include <cstddef>
#include <memory_resource>

namespace pot::memory
{

inline std::atomic<std::pmr::memory_resource *> g_coro_memory_resource{
    std::pmr::new_delete_resource()};

inline void set_memory_resource(std::pmr::memory_resource *resource) noexcept
{
    if (resource)
        g_coro_memory_resource.store(resource, std::memory_order_release);
}

inline void reset_memory_resource() noexcept
{
    g_coro_memory_resource.store(std::pmr::new_delete_resource(), std::memory_order_release);
}

[[nodiscard]] inline void *allocate(std::size_t size,
                                    std::size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__)
{
    auto *resource = g_coro_memory_resource.load(std::memory_order_acquire);
    return resource->allocate(size, alignment);
}

inline void deallocate(void *ptr, std::size_t size,
                       std::size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__) noexcept
{
    if (!ptr)
        return;

    auto *resource = g_coro_memory_resource.load(std::memory_order_acquire);
    resource->deallocate(ptr, size, alignment);
}

} // namespace pot::memory
