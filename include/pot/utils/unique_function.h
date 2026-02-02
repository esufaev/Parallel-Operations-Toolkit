#pragma once

#include <type_traits>
#include <utility>
#include <cstddef>
#include <cassert>
#include <cstring>

namespace pot::details
{
    struct func_constants
    {
        static constexpr size_t total_size = 64;
        static constexpr size_t buffer_size = total_size - sizeof(void *);
    };

    struct vtable
    {
        void (*move_destroy_fn)(void *src, void *dst) noexcept;
        void (*execute_destroy_fn)(void *target);
        void (*destroy_fn)(void *target) noexcept;

        vtable(const vtable &) noexcept = default;

        constexpr vtable() noexcept : move_destroy_fn(nullptr), execute_destroy_fn(nullptr), destroy_fn(nullptr) {}

        constexpr vtable(decltype(move_destroy_fn) move_destroy_fn,
                         decltype(execute_destroy_fn) execute_destroy_fn,
                         decltype(destroy_fn) destroy_fn) noexcept : move_destroy_fn(move_destroy_fn),
                                                                     execute_destroy_fn(execute_destroy_fn), destroy_fn(destroy_fn) {}

        static constexpr bool trivially_copiable_destructible(decltype(move_destroy_fn) move_fn) noexcept
        {
            return move_fn == nullptr;
        }

        static constexpr bool trivially_destructible(decltype(destroy_fn) destroy_fn) noexcept
        {
            return destroy_fn == nullptr;
        }
    };

    template <class callable_type>
    class callable_vtable
    {

    private:
        static callable_type *inline_ptr(void *src) noexcept
        {
            return static_cast<callable_type *>(src);
        }

        static callable_type *allocated_ptr(void *src) noexcept
        {
            return *static_cast<callable_type **>(src);
        }

        static callable_type *&allocated_ref_ptr(void *src) noexcept
        {
            return *static_cast<callable_type **>(src);
        }

        static void move_destroy_inline(void *src, void *dst) noexcept
        {
            auto callable_ptr = inline_ptr(src);
            new (dst) callable_type(std::move(*callable_ptr));
            callable_ptr->~callable_type();
        }

        static void move_destroy_allocated(void *src, void *dst) noexcept
        {
            auto callable_ptr = std::exchange(allocated_ref_ptr(src), nullptr);
            new (dst) callable_type *(callable_ptr);
        }

        static void execute_destroy_inline(void *target)
        {
            auto callable_ptr = inline_ptr(target);
            (*callable_ptr)();
            callable_ptr->~callable_type();
        }

        static void execute_destroy_allocated(void *target)
        {
            auto callable_ptr = allocated_ptr(target);
            (*callable_ptr)();
            delete callable_ptr;
        }

        static void destroy_inline(void *target) noexcept
        {
            auto callable_ptr = inline_ptr(target);
            callable_ptr->~callable_type();
        }

        static void destroy_allocated(void *target) noexcept
        {
            auto callable_ptr = allocated_ptr(target);
            delete callable_ptr;
        }

        static constexpr vtable make_vtable() noexcept
        {
            void (*move_destroy_fn)(void *src, void *dst) noexcept = nullptr;
            void (*destroy_fn)(void *target) noexcept = nullptr;

            if constexpr (std::is_trivially_copy_constructible_v<callable_type> && std::is_trivially_destructible_v<callable_type> &&
                          is_inlinable())
            {
                move_destroy_fn = nullptr;
            }
            else
            {
                move_destroy_fn = move_destroy;
            }

            if constexpr (std::is_trivially_destructible_v<callable_type> && is_inlinable())
            {
                destroy_fn = nullptr;
            }
            else
            {
                destroy_fn = destroy;
            }

            return vtable(move_destroy_fn, execute_destroy, destroy_fn);
        }

        template <class passed_callable_type>
        static void build_inlinable(void *dst, passed_callable_type &&callable)
        {
            new (dst) callable_type(std::forward<passed_callable_type>(callable));
        }

        template <class passed_callable_type>
        static void build_allocated(void *dst, passed_callable_type &&callable)
        {
            auto new_ptr = new callable_type(std::forward<passed_callable_type>(callable));
            new (dst) callable_type *(new_ptr);
        }

    public:
        static constexpr bool is_inlinable() noexcept
        {
            return std::is_nothrow_move_constructible_v<callable_type> && sizeof(callable_type) <= func_constants::buffer_size;
        }

        template <class passed_callable_type>
        static void build(void *dst, passed_callable_type &&callable)
        {
            if (is_inlinable())
            {
                return build_inlinable(dst, std::forward<passed_callable_type>(callable));
            }

            build_allocated(dst, std::forward<passed_callable_type>(callable));
        }

        static void move_destroy(void *src, void *dst) noexcept
        {
            assert(src != nullptr);
            assert(dst != nullptr);

            if (is_inlinable())
            {
                return move_destroy_inline(src, dst);
            }

            return move_destroy_allocated(src, dst);
        }

        static void execute_destroy(void *target)
        {
            assert(target != nullptr);

            if (is_inlinable())
            {
                return execute_destroy_inline(target);
            }

            return execute_destroy_allocated(target);
        }

        static void destroy(void *target) noexcept
        {
            assert(target != nullptr);

            if (is_inlinable())
            {
                return destroy_inline(target);
            }

            return destroy_allocated(target);
        }

        static constexpr callable_type *as(void *src) noexcept
        {
            if (is_inlinable())
            {
                return inline_ptr(src);
            }

            return allocated_ptr(src);
        }

        static constexpr inline vtable s_vtable = make_vtable();
    };

} // namespace pot::details

namespace pot::utils
{
    class unique_function_once
    {
        const details::vtable *m_vtbl = nullptr;
        alignas(std::max_align_t) std::byte m_buffer[details::func_constants::buffer_size]{};

    public:
        static constexpr std::size_t total_size = details::func_constants::total_size;
        static constexpr std::size_t buffer_size = details::func_constants::buffer_size;
        static_assert(sizeof(void *) + buffer_size == total_size, "layout invariant broken");

        constexpr unique_function_once() noexcept = default;

        unique_function_once(const unique_function_once &) = delete;
        unique_function_once &operator=(const unique_function_once &) = delete;

        template <typename T>
        using is_self_t = std::is_same<std::decay_t<T>, unique_function_once>;

        template <typename F, typename D = std::decay_t<F>>
        requires (!is_self_t<F>::value) && std::invocable<D&> && std::same_as<std::invoke_result_t<D&>, void>
        unique_function_once(F&& f) : m_vtbl(&details::callable_vtable<D>::s_vtable)
        {
            details::callable_vtable<D>::build(m_buffer, std::forward<F>(f));
        }

        unique_function_once(unique_function_once &&other) noexcept
        {
            move_from(std::move(other));
        }

        unique_function_once &operator=(unique_function_once &&other) noexcept
        {
            if (this != &other)
            {
                reset();
                move_from(std::move(other));
            }
            return *this;
        }

        ~unique_function_once() noexcept { reset(); }

        void operator()()
        {
            assert(m_vtbl && "attempt to call empty unique_function_once");
            auto *vt = std::exchange(m_vtbl, nullptr);
            vt->execute_destroy_fn(m_buffer);
        }

        void reset() noexcept
        {
            if (!m_vtbl)
                return;
            auto *vt = std::exchange(m_vtbl, nullptr);
            if (!details::vtable::trivially_destructible(vt->destroy_fn))
            {
                vt->destroy_fn(m_buffer);
            }
        }

        bool empty() const noexcept { return m_vtbl == nullptr; }
        explicit operator bool() const noexcept { return !empty(); }

    private:
        void move_from(unique_function_once &&other) noexcept
        {
            if (!other.m_vtbl)
            {
                m_vtbl = nullptr;
                return;
            }

            const details::vtable *vt = other.m_vtbl;

            if (details::vtable::trivially_copiable_destructible(vt->move_destroy_fn))
            {
                std::memcpy(m_buffer, other.m_buffer, buffer_size);
                other.m_vtbl = nullptr;
            }
            else
            {
                vt->move_destroy_fn(other.m_buffer, m_buffer);
                other.m_vtbl = nullptr;
            }

            m_vtbl = vt;
        }
    };

    template <class F>
    unique_function_once make_unique_function_once(F &&f)
    {
        return unique_function_once(std::forward<F>(f));
    }
} // namespace pot::utils