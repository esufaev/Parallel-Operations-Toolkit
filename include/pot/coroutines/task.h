#pragma once

#include <atomic>
#include <variant>
#include <coroutine>
#include <thread>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pot::coroutines::detail
{
    template <typename T>
    struct basic_promise_type
    {
        using value_type = T;
        using variant_type =
            std::conditional_t<std::is_void_v<T>,
                               std::variant<std::monostate, std::exception_ptr>,
                               std::variant<std::monostate, T, std::exception_ptr>>;

        struct final_awaiter
        {
            bool await_ready() const noexcept { return false; }
            template <typename PROMISE>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<PROMISE> h) const noexcept
            {
                return h.promise().m_continuation ? h.promise().m_continuation : std::noop_coroutine();
            }
            void await_resume() const noexcept {}
        };

        static std::suspend_always initial_suspend() noexcept { return {}; }
        static final_awaiter final_suspend() noexcept { return {}; }

        void unhandled_exception() { set_exception(std::current_exception()); }

        template <typename U = T>
            requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
        void set_value(U &&value)
        {
            m_data.template emplace<T>(std::forward<U>(value));
            if (m_ready.exchange(true, std::memory_order_release))
                throw std::runtime_error("Value already set in promise_type.");
        }

        void set_value()
            requires(std::is_void_v<T>)
        {
            m_data.template emplace<std::monostate>();
            if (m_ready.exchange(true, std::memory_order_release))
                throw std::runtime_error("Void value already set in promise_type.");
        }

        void set_exception(std::exception_ptr exception)
        {
            m_data.template emplace<std::exception_ptr>(exception);
            if (m_ready.exchange(true, std::memory_order_release))
                throw std::runtime_error("Exception already set in promise_type.");
        }

        bool is_ready() const noexcept { return m_ready.load(std::memory_order_acquire); }

        void wait()
        {
            while (!m_ready.load(std::memory_order_acquire))
                std::this_thread::yield();
        }

        T get()
        {
            wait();
            if (std::holds_alternative<std::exception_ptr>(m_data))
                std::rethrow_exception(std::get<std::exception_ptr>(m_data));

            if constexpr (std::is_void_v<T>)
                return;
            else
                return std::get<T>(m_data);
        }

        std::atomic<bool> m_ready{false};
        variant_type m_data{std::monostate{}};
        std::coroutine_handle<> m_continuation{};
    };
} // namespace pot::coroutines::detail

namespace pot::coroutines
{
    template <typename T>
    struct promise_type_impl;

    template <typename T>
    struct lazy_promise_type_impl;

    /**
     * @brief A coroutine return type that defers execution until awaited.
     *
     * @tparam T The result type (may be `void`).
     *
     * @return
     * - `co_await lazy_task<T>`: Suspends caller until the coroutine completes.  
     *   Returns the stored value (or nothing for `void`).
     * - `get()`: Runs to completion synchronously and returns the result.  
     * - `sync_wait()`: Busy-waits until ready and then returns the result.
     */
    template <typename T>
    class lazy_task
    {
    public:
        using promise_type = lazy_promise_type_impl<T>;
        using handle_type = std::coroutine_handle<promise_type>;
        using value_type = T;

        lazy_task() = default;
        explicit lazy_task(handle_type h) noexcept : m_handle(h) {}

        lazy_task(lazy_task const &) = delete;
        lazy_task &operator=(lazy_task const &) = delete;

        lazy_task(lazy_task &&other) noexcept : m_handle(other.m_handle) { other.m_handle = {}; }
        lazy_task &operator=(lazy_task &&other) noexcept
        {
            if (this != &other)
            {
                if (m_handle)
                    m_handle.destroy();
                m_handle = other.m_handle;
                other.m_handle = {};
            }
            return *this;
        }

        explicit operator bool() const noexcept { return m_handle && m_handle.done(); }

        bool await_ready() const noexcept
        {
            return !m_handle || m_handle.promise().is_ready();
        }

        template <typename PROMISE>
        void await_suspend(std::coroutine_handle<PROMISE> awaiting)
        {
            m_handle.promise().m_continuation = awaiting;
            if (!m_handle.done())
                m_handle.resume();
        }

        auto await_resume()
        {
            if constexpr (std::is_void_v<T>)
            {
                m_handle.promise().get();
                return;
            }
            else
            {
                return m_handle.promise().get();
            }
        }

        auto get()
        {
            if (m_handle && !m_handle.promise().is_ready())
                m_handle.resume();

            if constexpr (std::is_void_v<T>)
                m_handle.promise().get();
            else
                return m_handle.promise().get();
        }

        auto sync_wait()
        {
            m_handle.promise().wait();

            if constexpr (std::is_void_v<T>)
                m_handle.promise().get();
            else
                return m_handle.promise().get();
        }

        ~lazy_task()
        {
            if (m_handle)
                m_handle.destroy();
        }

    private:
        handle_type m_handle{};
    };

    /**
     * @brief A coroutine return type that starts execution immediately.
     *
     * `task<T>` behaves like a coroutine future:
     * - Execution begins right away (`initial_suspend = suspend_never`).
     * - Awaiting it will suspend until completion.
     *
     * @tparam T The result type (may be `void`).
     *
     * @param m_handle Internal coroutine handle managed by the task.
     *
     * @return
     * - `co_await task<T>`: Suspends caller until coroutine completes, then returns the result.
     * - `get()`: Executes until completion if not ready, then returns result synchronously.
     * - `sync_wait()`: Busy-waits until ready and returns result.
     *
     */
    template <typename T>
    class task
    {
    public:
        using promise_type = promise_type_impl<T>;
        using handle_type = std::coroutine_handle<promise_type>;
        using value_type = T;

        explicit task(handle_type handle) : m_handle(handle) {}

        task(task const &) = delete;
        task &operator=(task const &) = delete;

        task(task &&rhs) noexcept : m_handle(rhs.m_handle) { rhs.m_handle = nullptr; }
        task &operator=(task &&rhs) noexcept
        {
            if (this != &rhs)
            {
                if (m_handle)
                    m_handle.destroy();
                m_handle = rhs.m_handle;
                rhs.m_handle = nullptr;
            }
            return *this;
        }

        explicit operator bool() const noexcept { return m_handle && m_handle.done(); }

        bool await_ready() const noexcept { return !m_handle || m_handle.promise().is_ready(); }

        template <typename PROMISE>
        void await_suspend(std::coroutine_handle<PROMISE> awaiting)
        {
            m_handle.promise().m_continuation = awaiting;
            if (!m_handle.done())
                m_handle.resume();
        }

        auto await_resume()
        {
            if constexpr (std::is_void_v<T>)
            {
                m_handle.promise().get();
                return;
            }
            else
            {
                return m_handle.promise().get();
            }
        }

        auto get()
        {
            if (m_handle && !m_handle.promise().is_ready())
                m_handle.resume();

            if constexpr (std::is_void_v<T>)
                m_handle.promise().get();
            else
                return m_handle.promise().get();
        }

        auto sync_wait()
        {
            m_handle.promise().wait();

            if constexpr (std::is_void_v<T>)
                m_handle.promise().get();
            else
                return m_handle.promise().get();
        }

        ~task()
        {
            if (m_handle)
                m_handle.destroy();
        }

    private:
        handle_type m_handle{};
    };

    template <typename T>
    struct promise_type_impl : detail::basic_promise_type<T>
    {
        using task_type = task<T>;
        using handle_type = std::coroutine_handle<promise_type_impl<T>>;

        static std::suspend_never initial_suspend() noexcept { return {}; }
        using detail::basic_promise_type<T>::final_suspend;

        auto get_return_object() { return task_type{handle_type::from_promise(*this)}; }
        void return_value(T value) { this->set_value(std::move(value)); }
    };

    template <>
    struct promise_type_impl<void> : detail::basic_promise_type<void>
    {
        using task_type = task<void>;
        using handle_type = std::coroutine_handle<promise_type_impl<void>>;

        static std::suspend_never initial_suspend() noexcept { return {}; }
        using detail::basic_promise_type<void>::final_suspend;

        auto get_return_object() { return task_type{handle_type::from_promise(*this)}; }
        void return_void() { this->set_value(); }
    };

    template <typename T>
    struct lazy_promise_type_impl : detail::basic_promise_type<T>
    {
        using handle_type = std::coroutine_handle<lazy_promise_type_impl<T>>;

        static std::suspend_always initial_suspend() noexcept { return {}; }
        using detail::basic_promise_type<T>::final_suspend;

        auto get_return_object()
        {
            return lazy_task<T>{handle_type::from_promise(*this)};
        }

        template <typename U = T>
            requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
        void return_value(U &&value)
        {
            this->set_value(std::forward<U>(value));
        }
    };

    template <>
    struct lazy_promise_type_impl<void> : detail::basic_promise_type<void>
    {
        using handle_type = std::coroutine_handle<lazy_promise_type_impl<void>>;

        static std::suspend_always initial_suspend() noexcept { return {}; }
        using detail::basic_promise_type<void>::final_suspend;

        auto get_return_object()
        {
            return lazy_task<void>{handle_type::from_promise(*this)};
        }

        void return_void() { this->set_value(); }
    };

} // namespace pot::coroutines

namespace std
{
    template <typename T, typename... Args>
    struct coroutine_traits<pot::coroutines::task<T>, Args...>
    {
        using promise_type = typename pot::coroutines::task<T>::promise_type;
    };

    template <typename T, typename... Args>
    struct coroutine_traits<pot::coroutines::lazy_task<T>, Args...>
    {
        using promise_type = typename pot::coroutines::lazy_task<T>::promise_type;
    };
}

namespace pot::traits
{
    template <typename T>
    struct is_task : std::false_type
    {
    };
    template <typename U>
    struct is_task<pot::coroutines::task<U>> : std::true_type
    {
    };
    template <typename T>
    inline constexpr bool is_task_v = is_task<T>::value;

    template <typename T>
    struct is_lazy_task : std::false_type
    {
    };
    template <typename U>
    struct is_lazy_task<pot::coroutines::lazy_task<U>> : std::true_type
    {
    };
    template <typename T>
    inline constexpr bool is_lazy_task_v = is_lazy_task<T>::value;

    template <typename T>
    struct awaitable_value
    {
        using type = T;
    };
    template <typename U>
    struct awaitable_value<coroutines::task<U>>
    {
        using type = U;
    };
    template <typename U>
    struct awaitable_value<coroutines::lazy_task<U>>
    {
        using type = U;
    };
    template <typename T>
    using awaitable_value_t = typename awaitable_value<T>::type;
} // namespace pot
