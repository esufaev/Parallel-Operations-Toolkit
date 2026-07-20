#pragma once

#include <cstddef>
#include <utility>
#include <type_traits>
#include <new>
#include <functional>
#include <concepts>
#include <memory_resource> 

namespace pot::utils
{
    template<typename Signature>
    class function;

    template<typename R, typename... Args>
    class function<R(Args...)>
    {
    private:
        static constexpr size_t buffer_size = sizeof(void*) * 8;

        struct vtable
        {
            R (*invoke)(void*, Args...);
            void (*destroy)(void*, std::pmr::memory_resource*);
            void (*copy)(const void*, void*, std::pmr::memory_resource*);
            void (*move)(void*, void*, std::pmr::memory_resource*);
        };

        alignas(std::max_align_t) std::byte m_buffer[buffer_size];
        const vtable* m_vtable = nullptr;
        std::pmr::memory_resource* m_resource = std::pmr::get_default_resource(); 

        template <typename F>
        static constexpr bool is_small = (sizeof(F) <= buffer_size) &&
                                         (alignof(F) <= alignof(std::max_align_t)) &&
                                         std::is_nothrow_move_constructible_v<F>;

        template <typename F>
        struct small_model
        {
            static R invoke(void* buf, Args... args) {
                return (*reinterpret_cast<F*>(buf))(std::forward<Args>(args)...);
            }
            static void destroy(void* buf, std::pmr::memory_resource*) {
                reinterpret_cast<F*>(buf)->~F();
            }
            static void copy(const void* src, void* dst, std::pmr::memory_resource*) {
                new (dst) F(*reinterpret_cast<const F*>(src));
            }
            static void move(void* src, void* dst, std::pmr::memory_resource*) {
                F* src_ptr = reinterpret_cast<F*>(src);
                new (dst) F(std::move(*src_ptr));
                src_ptr->~F(); 
            }
            inline static constexpr vtable table = { invoke, destroy, copy, move };
        };

        template <typename F>
        struct large_model
        {
            static R invoke(void* buf, Args... args) {
                return (**reinterpret_cast<F**>(buf))(std::forward<Args>(args)...);
            }
            static void destroy(void* buf, std::pmr::memory_resource* res) {
                F* ptr = *reinterpret_cast<F**>(buf);
                ptr->~F();
                res->deallocate(ptr, sizeof(F), alignof(F)); 
            }
            static void copy(const void* src, void* dst, std::pmr::memory_resource* res) {
                F* src_ptr = *reinterpret_cast<F* const*>(src);
                void* mem = res->allocate(sizeof(F), alignof(F)); 
                try {
                    new (mem) F(*src_ptr);
                } catch(...) {
                    res->deallocate(mem, sizeof(F), alignof(F));
                    throw;
                }
                *reinterpret_cast<void**>(dst) = mem;
            }
            static void move(void* src, void* dst, std::pmr::memory_resource*) {
                *reinterpret_cast<F**>(dst) = *reinterpret_cast<F**>(src);
            }
            inline static constexpr vtable table = { invoke, destroy, copy, move };
        };

        template<typename F>
        static constexpr bool is_valid_callable = 
            !std::same_as<std::decay_t<F>, function> && 
            std::is_invocable_r_v<R, std::decay_t<F>&, Args...>;

    public:
        function() noexcept = default;
        function(std::nullptr_t) noexcept : m_vtable(nullptr) {}

        function(std::allocator_arg_t, std::pmr::memory_resource* res) noexcept 
            : m_resource(res) {}
            
        function(std::allocator_arg_t, std::pmr::memory_resource* res, std::nullptr_t) noexcept 
            : m_vtable(nullptr), m_resource(res) {}

        template<typename F>
        requires is_valid_callable<F>
        function(F&& func) : function(std::allocator_arg, std::pmr::get_default_resource(), std::forward<F>(func)) {}

        template<typename F>
        requires is_valid_callable<F>
        function(std::allocator_arg_t, std::pmr::memory_resource* res, F&& func) 
            : m_resource(res)
        {
            using Decayed = std::decay_t<F>;
            if constexpr (is_small<Decayed>) 
            {
                m_vtable = &small_model<Decayed>::table;
                new (&m_buffer) Decayed(std::forward<F>(func));
            } 
            else 
            {
                m_vtable = &large_model<Decayed>::table;
                void* mem = m_resource->allocate(sizeof(Decayed), alignof(Decayed));
                try {
                    new (mem) Decayed(std::forward<F>(func));
                } catch(...) {
                    m_resource->deallocate(mem, sizeof(Decayed), alignof(Decayed));
                    throw;
                }
                *reinterpret_cast<void**>(&m_buffer) = mem;
            }
        }

        ~function()
        {
            if (m_vtable) m_vtable->destroy(&m_buffer, m_resource);
        }

        function(const function& other) 
            : function(std::allocator_arg, std::pmr::get_default_resource(), other) {}

        function(std::allocator_arg_t, std::pmr::memory_resource* res, const function& other) 
            : m_vtable(other.m_vtable), m_resource(res)
        {
            if (m_vtable) m_vtable->copy(&other.m_buffer, &m_buffer, m_resource);
        }

        function(function&& other) noexcept 
            : m_vtable(other.m_vtable), m_resource(other.m_resource)
        {
            if (m_vtable) {
                m_vtable->move(&other.m_buffer, &m_buffer, m_resource);
                other.m_vtable = nullptr; 
            }
        }

        function(std::allocator_arg_t, std::pmr::memory_resource* res, function&& other)
            : m_resource(res)
        {
            if (m_resource == other.m_resource) {
                m_vtable = other.m_vtable;
                if (m_vtable) {
                    m_vtable->move(&other.m_buffer, &m_buffer, m_resource);
                    other.m_vtable = nullptr;
                }
            } else {
                m_vtable = other.m_vtable;
                if (m_vtable) {
                    m_vtable->copy(&other.m_buffer, &m_buffer, m_resource);
                    
                    other.m_vtable->destroy(&other.m_buffer, other.m_resource);
                    other.m_vtable = nullptr;
                }
            }
        }

        function& operator=(const function& other)
        {
            if (this != &other) 
            {
                if (m_vtable) m_vtable->destroy(&m_buffer, m_resource);
                m_vtable = other.m_vtable;
                if (m_vtable) m_vtable->copy(&other.m_buffer, &m_buffer, m_resource);
            }
            return *this;
        }

        function& operator=(function&& other)
        {
            if (this != &other) 
            {
                if (m_vtable) m_vtable->destroy(&m_buffer, m_resource);
                
                if (m_resource == other.m_resource) {
                    m_vtable = other.m_vtable;
                    if (m_vtable) {
                        m_vtable->move(&other.m_buffer, &m_buffer, m_resource);
                        other.m_vtable = nullptr;
                    }
                } else {
                    m_vtable = other.m_vtable;
                    if (m_vtable) {
                        m_vtable->copy(&other.m_buffer, &m_buffer, m_resource);
                        
                        other.m_vtable->destroy(&other.m_buffer, other.m_resource);
                        other.m_vtable = nullptr;
                    }
                }
            }
            return *this;
        }

        function& operator=(std::nullptr_t) noexcept
        {
            if (m_vtable) 
            {
                m_vtable->destroy(&m_buffer, m_resource);
                m_vtable = nullptr;
            }
            return *this;
        }

        R operator()(Args... args)
        {
            if (!m_vtable) throw std::bad_function_call();
            return m_vtable->invoke(&m_buffer, std::forward<Args>(args)...);
        }

        explicit operator bool() const noexcept { return m_vtable != nullptr; }

        std::pmr::memory_resource* get_memory_resource() const noexcept {
            return m_resource;
        }
    };
}
