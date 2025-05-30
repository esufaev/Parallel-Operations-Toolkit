#pragma once

#include "pot/coroutines/__details/basic_promise_type.h"

namespace pot::coroutines
{
    using coro_t = std::coroutine_handle<>;

    template <typename T, bool Lazy>
    class [[nodiscard]] basic_task
    {
    public:
        struct promise_type : public pot::coroutine::__details::basic_promise_type<T>
        {
            basic_task<T, Lazy> get_return_object() { return basic_task{std::coroutine_handle<promise_type>::from_promise(*this)}; }

            template <typename U> requires std::convertible_to<U, T>
            void return_value(U &&value) { this->set_value(std::forward<U>(value)); }
            void unhandled_exception() { this->set_exception(std::current_exception()); }

            static constexpr std::suspend_never initial_suspend() noexcept requires(!Lazy) { return {}; }
            static constexpr std::suspend_always initial_suspend() noexcept requires(Lazy) { return {}; }

            constexpr auto final_suspend() noexcept 
            {
                struct awaiter
                {
                    std::coroutine_handle<> continuation;
                    bool await_ready() const noexcept { return false; }
                    void await_suspend(std::coroutine_handle<>) const noexcept
                    {
                        if (continuation)
                            continuation.resume();
                    }
                    void await_resume() const noexcept {}
                };
                return awaiter{ this->m_continuation };
            }
        };

        using handle_type = std::coroutine_handle<promise_type>;
        using value_type = T;

        explicit basic_task(handle_type h) noexcept : m_handle(h) {}

        operator bool() const noexcept { return &m_handle; }

        basic_task() noexcept : m_handle(nullptr) {}
        basic_task(basic_task &&rhs) noexcept
        {
            if (rhs.m_handle)
            {
                m_handle = rhs.m_handle;
                rhs.m_handle = nullptr;
            }
        }
        basic_task &operator=(basic_task &&rhs) noexcept
        {
            std::printf("operator=\n\r");
            if (this != &rhs)
            {
                m_handle = rhs.m_handle;
                rhs.m_handle = nullptr;
            }
            return *this;
        }

        basic_task(const basic_task &) = delete;
        basic_task &operator=(const basic_task &) = delete;

        ~basic_task()
        {
            if (m_handle)
                m_handle.destroy();
        }

        bool await_ready() const noexcept { return m_handle && m_handle.done(); }
        void await_suspend(coro_t continuation) noexcept
        {
            if (!m_handle)
            {
                m_handle.promise().set_continuation(continuation);
                return;
            }

            try
            {
                m_handle.promise().set_continuation(continuation);
                if (!m_handle.done()) { m_handle.resume(); }
                else { continuation.resume(); }
            }
            catch (...)
            {
                m_handle.promise().set_exception(std::current_exception());
                continuation.resume();
            }
        }
        T await_resume()
        {
            if (!m_handle)
                throw std::runtime_error("Invalid coroutine m_handle");

            if constexpr (std::is_void_v<T>)
                m_handle.promise().get();
            else
                return m_handle.promise().get();
        }

        T get()
        {
            if (Lazy && m_handle && !m_handle.done()) { m_handle.resume(); }
            return await_resume();
        }

        class iterator
        {
            handle_type m_handle;

        public:
            using value_type = T;
            using difference_type = std::ptrdiff_t;

            iterator(handle_type h = nullptr) : m_handle(h) {}

            value_type operator*() const { return m_handle.promise().current_value(); }
            iterator &operator++()
            {
                m_handle.resume();
                return *this;
            }
            bool operator==(const iterator &) const = default;
        };

        auto begin() { return iterator(m_handle); }
        auto end() { return iterator(); }

    private:
        handle_type m_handle;
    };

    template <>
    struct basic_task<void, true>::promise_type : public pot::coroutine::__details::basic_promise_type<void>
    {
        basic_task<void, true> get_return_object() { return basic_task<void, true>{std::coroutine_handle<promise_type>::from_promise(*this)}; }

        void return_void() { this->set_value(); }
        void unhandled_exception() { this->set_exception(std::current_exception()); }

        static constexpr std::suspend_always initial_suspend() noexcept { return {}; }
        constexpr auto final_suspend() noexcept 
        {
            struct awaiter
            {
                std::coroutine_handle<> continuation;
                bool await_ready() const noexcept { return false; }
                void await_suspend(std::coroutine_handle<>) const noexcept
                {
                    if (continuation)
                        continuation.resume();
                }
                void await_resume() const noexcept {}
            };
            return awaiter{ this->m_continuation };
        }
    };

    template <>
    struct basic_task<void, false>::promise_type : public pot::coroutine::__details::basic_promise_type<void>
    {
        basic_task<void, false> get_return_object() { return basic_task<void, false>{std::coroutine_handle<promise_type>::from_promise(*this)}; }

        void return_void() { this->set_value(); }
        void unhandled_exception() { this->set_exception(std::current_exception()); }

        static constexpr std::suspend_never initial_suspend() noexcept { return {}; }
        constexpr auto final_suspend() noexcept 
        {
            struct awaiter
            {
                std::coroutine_handle<> continuation;
                bool await_ready() const noexcept { return false; }
                void await_suspend(std::coroutine_handle<>) const noexcept
                {
                    if (continuation)
                        continuation.resume();
                }
                void await_resume() const noexcept {}
            };
            return awaiter{ this->m_continuation };
        }
    };

    template <typename T>
    using task = basic_task<T, false>;

    template <typename T>
    using lazy_task = basic_task<T, true>;
}

namespace pot::traits
{
    template <typename T>
    struct task_value_type  
    {
        using type = T;
    };

    template <typename ValueType, bool Lazy>
    struct task_value_type<coroutines::basic_task<ValueType, Lazy>>
    {
        using type = ValueType;
    };

    template <typename ValueType>
    using task_value_type_t = typename task_value_type<ValueType>::type;

    template <typename T>
    struct awaitable_value_type
    {
        using type = T;
    };

    template <template <typename, bool> typename Task, typename T, bool Lazy>
    struct awaitable_value_type<Task<T, Lazy>>
    {
        using type = T;
    };

    template <typename T>
    using awaitable_value_type_t = typename awaitable_value_type<T>::type;

    template <typename T>
    struct is_basic_task : std::false_type
    {
    };

    template <typename T, bool Lazy>
    struct is_basic_task<pot::coroutines::basic_task<T, Lazy>> : std::true_type
    {
    };

    namespace concepts
    {
        template <typename Type>
        concept is_task = is_basic_task<Type>::value;
    }
}

static_assert(!pot::traits::concepts::is_task<int>);
static_assert(!pot::traits::concepts::is_task<std::string>);
static_assert(pot::traits::concepts::is_task<pot::coroutines::basic_task<int, true>>);
