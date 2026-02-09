#pragma once

#include <atomic>
#include <coroutine>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

#include "pot/memory/coro_memory.h"

namespace pot::coroutines::detail
{
template <typename T> struct basic_promise_type
{
    using value_type = T;
    using variant_type =
        std::conditional_t<std::is_void_v<T>, std::variant<std::monostate, std::exception_ptr>,
                           std::variant<std::monostate, T, std::exception_ptr>>;

    void unhandled_exception() { set_exception(std::current_exception()); }

    void *operator new(std::size_t size) { return pot::memory::get_coro_pool().allocate(size); }

    void operator delete(void *ptr, std::size_t size)
    {
        pot::memory::get_coro_pool().deallocate(ptr, size);
    }

    template <typename U = T>
        requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
    void set_value(U &&value)
    {
        m_data.template emplace<T>(std::forward<U>(value));
    }

    void set_value()
        requires(std::is_void_v<T>)
    {
        m_data.template emplace<std::monostate>();
    }

    void set_exception(std::exception_ptr exception)
    {
        m_data.template emplace<std::exception_ptr>(exception);
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

    struct final_awaiter
    {
        bool await_ready() const noexcept { return false; }

        template <typename PROMISE>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<PROMISE> h) noexcept
        {
            void *const sentinel = reinterpret_cast<void *>(1);

            void *prev = h.promise().m_awaiter.exchange(sentinel, std::memory_order_acq_rel);

            std::coroutine_handle<> next = std::noop_coroutine();

            if (prev != nullptr && prev != sentinel)
            {
                next = std::coroutine_handle<>::from_address(prev);
            }

            h.promise().m_ready.store(true, std::memory_order_release);

            return next;
        }

        void await_resume() noexcept {}
    };

    std::atomic<bool> m_ready{false};
    std::atomic<void *> m_awaiter{nullptr};
    variant_type m_data{std::monostate{}};
};
} // namespace pot::coroutines::detail

namespace pot::coroutines
{
template <typename T> struct task_promise_type_impl;

template <typename T> class task
{
  public:
    using promise_type = task_promise_type_impl<T>;
    using handle_type = std::coroutine_handle<promise_type>;
    using value_type = T;

    explicit task(handle_type handle) noexcept : m_handle(handle) {}

    task(task const &) = delete;
    task &operator=(task const &) = delete;

    task(task &&other) noexcept : m_handle(other.m_handle) { other.m_handle = {}; }
    task &operator=(task &&other) noexcept
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

    bool await_ready() const noexcept
    {
        return !m_handle || m_handle.done() || m_handle.promise().is_ready();
    }

    bool await_suspend(std::coroutine_handle<> next)
    {
        if (!m_handle)
            throw std::runtime_error("m_handle does not exist");

        void *const sentinel = reinterpret_cast<void *>(1);
        void *expected = nullptr;

        if (m_handle.promise().m_awaiter.compare_exchange_strong(expected, next.address(),
                                                                 std::memory_order_acq_rel))
        {
            return true;
        }

        if (expected == sentinel)
        {
            return false;
        }

        throw std::runtime_error("Race condition or multiple awaiters detected");
    }

    T await_resume()
    {
        if constexpr (std::is_void_v<T>)
        {
            m_handle.promise().get();
            m_handle.destroy();
            m_handle = nullptr;
            return;
        }
        else
        {
            auto result = m_handle.promise().get();
            m_handle.destroy();
            m_handle = nullptr;
            return result;
        }
    }

    auto get()
    {
        if (!m_handle)
        {
            throw std::runtime_error("Coroutine is invalid");
        }

        if constexpr (std::is_void_v<T>)
        {
            try
            {
                m_handle.promise().get();
            }
            catch (...)
            {
                m_handle.destroy();
                m_handle = nullptr;
                throw;
            }
            m_handle.destroy();
            m_handle = nullptr;
            return;
        }
        else
        {
            try
            {
                auto result = m_handle.promise().get();
                m_handle.destroy();
                m_handle = nullptr;
                return result;
            }
            catch (...)
            {
                m_handle.destroy();
                m_handle = nullptr;
                throw;
            }
        }
    }

    ~task()
    {
        if (m_handle)
            m_handle.destroy();
    }

  private:
    handle_type m_handle{};
};

template <typename T> struct lazy_task_promise_type_impl;

template <typename T> class lazy_task
{
  public:
    using promise_type = lazy_task_promise_type_impl<T>;
    using handle_type = std::coroutine_handle<promise_type>;
    using value_type = T;

    explicit lazy_task(handle_type handle) noexcept : m_handle(handle) {}

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

    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> next)
    {
        if (m_handle && !m_handle.done())
        {
            m_handle.promise().m_awaiter.store(next.address(), std::memory_order_relaxed);
            return m_handle;
        }
        throw std::runtime_error("m_handle does not exist or done");
    }

    T await_resume()
    {
        if constexpr (std::is_void_v<T>)
        {
            m_handle.promise().get();
            m_handle.destroy();
            m_handle = nullptr;
            return;
        }
        else
        {
            auto result = m_handle.promise().get();
            m_handle.destroy();
            m_handle = nullptr;
            return result;
        }
    }

    auto get()
    {
        if (!m_handle || m_handle.done())
        {
            throw std::runtime_error("Coroutine is invalid or already done");
        }

        m_handle.resume();

        if constexpr (std::is_void_v<T>)
        {
            try
            {
                m_handle.promise().get();
            }
            catch (...)
            {
                m_handle.destroy();
                m_handle = nullptr;
                throw;
            }
            m_handle.destroy();
            m_handle = nullptr;
            return;
        }
        else
        {
            try
            {
                auto result = m_handle.promise().get();
                m_handle.destroy();
                m_handle = nullptr;
                return result;
            }
            catch (...)
            {
                m_handle.destroy();
                m_handle = nullptr;
                throw;
            }
        }
    }

    ~lazy_task()
    {
        if (m_handle)
            m_handle.destroy();
    }

  private:
    handle_type m_handle{};
};

template <typename T> struct task_promise_type_impl final : detail::basic_promise_type<T>
{
    using handle_type = std::coroutine_handle<task_promise_type_impl<T>>;

    static std::suspend_never initial_suspend() noexcept { return {}; }

    static auto final_suspend() noexcept
    {
        return typename detail::basic_promise_type<T>::final_awaiter{};
    }

    auto get_return_object() { return task<T>{handle_type::from_promise(*this)}; }

    template <typename U = T>
        requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
    void return_value(U &&value)
    {
        this->set_value(std::forward<U>(value));
    }
};

template <> struct task_promise_type_impl<void> final : detail::basic_promise_type<void>
{
    using handle_type = std::coroutine_handle<task_promise_type_impl<void>>;

    static std::suspend_never initial_suspend() noexcept { return {}; }

    static auto final_suspend() noexcept
    {
        return typename detail::basic_promise_type<void>::final_awaiter{};
    }

    auto get_return_object() { return task<void>{handle_type::from_promise(*this)}; }

    void return_void() { this->set_value(); }
};

template <typename T> struct lazy_task_promise_type_impl final : detail::basic_promise_type<T>
{
    using handle_type = std::coroutine_handle<lazy_task_promise_type_impl<T>>;

    static std::suspend_always initial_suspend() noexcept { return {}; }
    static auto final_suspend() noexcept
    {
        return typename detail::basic_promise_type<T>::final_awaiter{};
    }

    auto get_return_object() { return lazy_task<T>{handle_type::from_promise(*this)}; }

    template <typename U = T>
        requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
    void return_value(U &&value)
    {
        this->set_value(std::forward<U>(value));
    }
};

template <> struct lazy_task_promise_type_impl<void> final : detail::basic_promise_type<void>
{
    using handle_type = std::coroutine_handle<lazy_task_promise_type_impl<void>>;

    static std::suspend_always initial_suspend() noexcept { return {}; }
    static auto final_suspend() noexcept
    {
        return typename detail::basic_promise_type<void>::final_awaiter{};
    }

    auto get_return_object() { return lazy_task<void>{handle_type::from_promise(*this)}; }

    void return_void() { this->set_value(); }
};

} // namespace pot::coroutines

namespace std
{

template <typename T, typename... Args>
struct coroutine_traits<pot::coroutines::lazy_task<T>, Args...>
{
    using promise_type = typename pot::coroutines::lazy_task<T>::promise_type;
};
} // namespace std

namespace pot::traits
{
template <typename T> struct is_lazy_task : std::false_type
{
};
template <typename U> struct is_lazy_task<pot::coroutines::lazy_task<U>> : std::true_type
{
};
template <typename T> inline constexpr bool is_lazy_task_v = is_lazy_task<T>::value;

template <typename T> struct awaitable_value
{
    using type = T;
};
template <typename U> struct awaitable_value<coroutines::lazy_task<U>>
{
    using type = U;
};
template <typename T> using awaitable_value_t = typename awaitable_value<T>::type;
template <typename T> struct is_task : std::false_type
{
};
template <typename U> struct is_task<pot::coroutines::task<U>> : std::true_type
{
};
template <typename T> inline constexpr bool is_task_v = is_task<T>::value;

template <typename U> struct awaitable_value<coroutines::task<U>>
{
    using type = U;
};
} // namespace pot::traits
