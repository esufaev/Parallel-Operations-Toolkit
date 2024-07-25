#pragma once

#include <memory>
#include <cstdlib>
#include <limits>
#include <new>
#include <type_traits>
#include <stdexcept>

namespace pot::allocators
{
    template <typename T>
    class shared_allocator
    {
    public:
        using value_type = T;

        shared_allocator() = default;

        template <typename U>
        shared_allocator(const shared_allocator<U> &) noexcept {}

        T *allocate(std::size_t n)
        {
            if (n == 0)
                return nullptr;

            if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
                throw std::bad_alloc();

            void *ptr = std::malloc(n * sizeof(T));
            if (!ptr)
                throw std::bad_alloc();

            return static_cast<T *>(ptr);
        }

        void deallocate(T *ptr, std::size_t) noexcept
        {
            std::free(ptr);
        }

        template <typename U, typename... Args>
        void construct(U *p, Args &&...args)
        {
            ::new (static_cast<void *>(p)) U(std::forward<Args>(args)...);
        }

        template <typename U>
        void destroy(U *p) noexcept
        {
            p->~U();
        }

        template <typename U>
        struct rebind
        {
            using other = shared_allocator<U>;
        };

        bool operator==(const shared_allocator &) const noexcept
        {
            return *this == shared_allocator;
        }

        bool operator!=(const shared_allocator &other) const noexcept
        {
            return !(*this == other);
        }
    };
}
