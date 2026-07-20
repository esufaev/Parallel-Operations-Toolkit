#pragma once

#include <atomic>
#include <chrono>
#include <coroutine>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

#include "pot/memory/coro_memory.h"
#include "pot/utils/meta.h"
#include "pot/utils/progress.h"

#if defined(__clang__) && __clang_major__ >= 18
#define POT_CORO_ELIDABLE [[clang::coro_await_elidable]]
#define POT_CORO_ELIDABLE_ARG [[clang::coro_await_elidable_argument]]
#define POT_CORO_ONLY_DESTROY [[clang::coro_only_destroy_when_complete]]
#else
#define POT_CORO_ELIDABLE
#define POT_CORO_ELIDABLE_ARG
#define POT_CORO_ONLY_DESTROY
#endif

namespace pot
{
class executor;
}

namespace pot::coroutines
{
struct get_progress_awaiter
{
    std::shared_ptr<pot::coroutines::details::progress> m_progress;

    bool await_ready() const noexcept
    {
        return false;
    }

    template <typename PROMISE> bool await_suspend(std::coroutine_handle<PROMISE> handle) noexcept
    {
        if (!handle.promise().meta.m_progress)
        {
            handle.promise().meta.m_progress = std::make_shared<pot::coroutines::details::progress>();
        }
        m_progress = handle.promise().meta.m_progress;

        return false;
    }

    std::shared_ptr<pot::coroutines::details::progress> await_resume() noexcept
    {
        return std::move(m_progress);
    }
};

inline get_progress_awaiter get_progress()
{
    return {};
}
} // namespace pot::coroutines

namespace pot::coroutines::detail
{
template <typename T> struct basic_promise_type
{
    using value_type = T;
    using variant_type = std::conditional_t<std::is_void_v<T>, std::variant<std::monostate, std::exception_ptr>,
                                            std::variant<std::monostate, T, std::exception_ptr>>;

    void unhandled_exception() noexcept
    {
        set_exception(std::current_exception());
    }

    static void *operator new(std::size_t size)
    {
        return pot::memory::allocate(size, pot::cache_line_alignment);
    }
    static void *operator new(std::size_t size, std::align_val_t alignment)
    {
        return pot::memory::allocate(size, static_cast<std::size_t>(alignment));
    }
    static void operator delete(void *ptr, std::size_t size) noexcept
    {
        pot::memory::deallocate(ptr, size, pot::cache_line_alignment);
    }
    static void operator delete(void *ptr, std::size_t size, std::align_val_t alignment) noexcept
    {
        pot::memory::deallocate(ptr, size, static_cast<std::size_t>(alignment));
    }

    template <typename U = T>
        requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
    void set_value(U &&value) noexcept(std::is_nothrow_constructible_v<T, decltype(std::forward<U>(value))>)
    {
        m_data.template emplace<T>(std::forward<U>(value));
    }

    void set_value() noexcept
        requires(std::is_void_v<T>)
    {
        m_data.template emplace<std::monostate>();
    }
    void set_exception(std::exception_ptr exception) noexcept
    {
        m_data.template emplace<std::exception_ptr>(exception);
    }

    bool is_ready() const noexcept
    {
        return m_ready.load(std::memory_order_acquire);
    }

    void wait()
    {
        while (!m_ready.load(std::memory_order_acquire))
        {
            if (!meta.try_steal())
                std::this_thread::yield();
        }
    }

    template <class Rep, class Period, class Callback>
    void wait(const std::chrono::duration<Rep, Period> &interval, Callback &&cb)
    {
        auto next_cb_time = std::chrono::steady_clock::now() + interval;

        while (!m_ready.load(std::memory_order_acquire))
        {
            if (!meta.try_steal())
                std::this_thread::yield();

            auto now = std::chrono::steady_clock::now();
            if (now >= next_cb_time)
            {
                cb();
                next_cb_time = now + interval;
            }
        }
    }

    T extract_value()
    {
        if (std::holds_alternative<std::exception_ptr>(m_data))
            std::rethrow_exception(std::get<std::exception_ptr>(m_data));

        if constexpr (std::is_void_v<T>)
            return;
        else
            return std::move(std::get<T>(m_data));
    }

    T get()
    {
        wait();
        return extract_value();
    }

    template <class Rep, class Period, class Callback>
    T get(const std::chrono::duration<Rep, Period> &interval, Callback &&cb)
    {
        wait(interval, std::forward<Callback>(cb));
        return extract_value();
    }

    struct final_awaiter
    {
        bool await_ready() const noexcept
        {
            return false;
        }

        template <typename PROMISE> std::coroutine_handle<> await_suspend(std::coroutine_handle<PROMISE> h) noexcept
        {
            void *const sentinel = reinterpret_cast<void *>(1);
            void *prev = h.promise().m_awaiter.exchange(sentinel, std::memory_order_acq_rel);
            std::coroutine_handle<> next = std::noop_coroutine();

            if (prev != nullptr)
                next = std::coroutine_handle<>::from_address(prev);

            h.promise().m_ready.store(true, std::memory_order_release);
            h.promise().m_ready.notify_all();

            return next;
        }

        void await_resume() noexcept {}
    };

    alignas(pot::cache_line_alignment) std::atomic<bool> m_ready{false};
    std::atomic<void *> m_awaiter{nullptr};
    std::atomic<bool> m_has_waiter{false};

    alignas(pot::cache_line_alignment) pot::coroutines::details::task_meta meta;

    alignas(pot::cache_line_alignment) variant_type m_data{std::monostate{}};
};
} // namespace pot::coroutines::detail

namespace pot::coroutines
{
template <typename T> struct task_promise_type_impl;

template <typename T> class POT_CORO_ELIDABLE POT_CORO_ONLY_DESTROY task
{
  public:
    using promise_type = task_promise_type_impl<T>;
    using handle_type = std::coroutine_handle<promise_type>;
    using value_type = T;

    explicit task(handle_type handle) noexcept : m_handle(handle) {}

    task(task const &) = delete;
    task &operator=(task const &) = delete;

    task(task &&other) noexcept : m_handle(other.m_handle)
    {
        other.m_handle = {};
    }

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

    struct awaiter
    {
        handle_type m_handle;
        pot::coroutines::details::task_meta *m_caller_meta{nullptr};

        bool await_ready() const noexcept
        {
            return false;
        }

        template <typename PromiseType> bool await_suspend(std::coroutine_handle<PromiseType> next)
        {
            if (!m_handle)
                throw std::runtime_error("m_handle does not exist");

            if constexpr (requires { next.promise().meta; })
            {
                m_caller_meta = &next.promise().meta;

                next.promise().meta.run_count.fetch_add(1, std::memory_order_relaxed);

                auto &caller_prog = next.promise().meta.m_progress;
                auto &callee_prog = m_handle.promise().meta.m_progress;

                if (caller_prog && !callee_prog)
                    callee_prog = caller_prog;
                else if (!caller_prog && callee_prog)
                    caller_prog = callee_prog;
            }

            void *const sentinel = reinterpret_cast<void *>(1);
            void *expected = nullptr;

            if (m_handle.promise().m_awaiter.compare_exchange_strong(expected, next.address(),
                                                                     std::memory_order_acq_rel))
            {
                return true;
            }

            if (expected == sentinel)
                return false;
            std::unreachable();
        }

        T await_resume()
        {
            if (m_caller_meta)
            {
                auto &caller_prog = m_caller_meta->m_progress;
                auto &callee_prog = m_handle.promise().meta.m_progress;
                if (!caller_prog && callee_prog)
                    caller_prog = callee_prog;
            }
            return m_handle.promise().get();
        }
    };

    auto operator co_await() const noexcept
    {
        return awaiter{m_handle};
    }

    std::shared_ptr<pot::coroutines::details::progress> get_progress()
    {
        if (!m_handle)
            return nullptr;
        if (!m_handle.promise().meta.m_progress)
        {
            m_handle.promise().meta.m_progress = std::make_shared<pot::coroutines::details::progress>();
        }
        return m_handle.promise().meta.m_progress;
    }

    T get()
    {
        if (!m_handle)
            throw std::runtime_error("Coroutine is invalid");
        return m_handle.promise().get();
    }

    template <typename ExecType> T get(ExecType *executor)
    {
        if (!m_handle)
            throw std::runtime_error("Coroutine is invalid");
        if (executor)
        {
            m_handle.promise().meta.m_executor_ptr = executor;
            m_handle.promise().meta.m_steal_func = [](void *exec_ptr)
            { return static_cast<ExecType *>(exec_ptr)->try_steal(); };
        }
        return m_handle.promise().get();
    }

    template <typename Rep, typename Period, typename Callback>
    T get(const std::chrono::duration<Rep, Period> &interval, Callback &&cb)
    {
        if (!m_handle)
            throw std::runtime_error("Coroutine is invalid");
        return m_handle.promise().get(interval, std::forward<Callback>(cb));
    }

    template <typename Rep, typename Period, typename Callback, typename ExecType>
    T get(const std::chrono::duration<Rep, Period> &interval, Callback &&cb, ExecType *executor)
    {
        if (!m_handle)
            throw std::runtime_error("Coroutine is invalid");
        if (executor)
        {
            m_handle.promise().meta.m_executor_ptr = executor;
            m_handle.promise().meta.m_steal_func = [](void *exec_ptr)
            { return static_cast<ExecType *>(exec_ptr)->try_steal(); };
        }
        return m_handle.promise().get(interval, std::forward<Callback>(cb));
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

template <typename T> class POT_CORO_ELIDABLE POT_CORO_ONLY_DESTROY lazy_task
{
  public:
    using promise_type = lazy_task_promise_type_impl<T>;
    using handle_type = std::coroutine_handle<promise_type>;
    using value_type = T;

    explicit lazy_task(handle_type handle) noexcept : m_handle(handle) {}

    lazy_task(lazy_task const &) = delete;
    lazy_task &operator=(lazy_task const &) = delete;

    lazy_task(lazy_task &&other) noexcept : m_handle(other.m_handle)
    {
        other.m_handle = {};
    }

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

    struct awaiter
    {
        handle_type m_handle;
        pot::coroutines::details::task_meta *m_caller_meta{nullptr};

        bool await_ready() const noexcept
        {
            return false;
        }

        template <typename PromiseType> std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> next)
        {
            if constexpr (requires { next.promise().meta; })
            {
                m_caller_meta = &next.promise().meta;

                uint64_t parent_prio = next.promise().meta.run_count.load(std::memory_order_relaxed);
                m_handle.promise().meta.run_count.store(parent_prio + 1, std::memory_order_relaxed);

                auto &caller_prog = next.promise().meta.m_progress;
                auto &callee_prog = m_handle.promise().meta.m_progress;

                if (caller_prog && !callee_prog)
                    callee_prog = caller_prog;
                else if (!caller_prog && callee_prog)
                    caller_prog = callee_prog;
            }

            if (m_handle && !m_handle.done())
            {
                m_handle.promise().m_awaiter.store(next.address(), std::memory_order_relaxed);
                return m_handle;
            }
            std::unreachable();
        }

        T await_resume()
        {
            if (m_caller_meta)
            {
                auto &caller_prog = m_caller_meta->m_progress;
                auto &callee_prog = m_handle.promise().meta.m_progress;
                if (!caller_prog && callee_prog)
                    caller_prog = callee_prog;
            }
            return m_handle.promise().get();
        }
    };

    auto operator co_await() const noexcept
    {
        return awaiter{m_handle};
    }

    std::shared_ptr<pot::coroutines::details::progress> get_progress()
    {
        if (!m_handle)
            return nullptr;
        if (!m_handle.promise().meta.m_progress)
        {
            m_handle.promise().meta.m_progress = std::make_shared<pot::coroutines::details::progress>();
        }
        return m_handle.promise().meta.m_progress;
    }

    T get()
    {
        if (!m_handle || m_handle.done())
            throw std::runtime_error("Coroutine is invalid or already done");
        m_handle.resume();
        return m_handle.promise().get();
    }

    template <typename ExecType> T get(ExecType *executor)
    {
        if (!m_handle || m_handle.done())
            throw std::runtime_error("Coroutine is invalid or already done");
        if (executor)
        {
            m_handle.promise().meta.m_executor_ptr = executor;
            m_handle.promise().meta.m_steal_func = [](void *exec_ptr)
            { return static_cast<ExecType *>(exec_ptr)->try_steal(); };
        }
        m_handle.resume();
        return m_handle.promise().get();
    }

    template <typename Rep, typename Period, typename Callback>
    T get(const std::chrono::duration<Rep, Period> &interval, Callback &&cb)
    {
        if (!m_handle || m_handle.done())
            throw std::runtime_error("Coroutine is invalid or already done");
        m_handle.resume();
        return m_handle.promise().get(interval, std::forward<Callback>(cb));
    }

    template <typename Rep, typename Period, typename Callback, typename ExecType>
    T get(const std::chrono::duration<Rep, Period> &interval, Callback &&cb, ExecType *executor)
    {
        if (!m_handle || m_handle.done())
            throw std::runtime_error("Coroutine is invalid or already done");
        if (executor)
        {
            m_handle.promise().meta.m_executor_ptr = executor;
            m_handle.promise().meta.m_steal_func = [](void *exec_ptr)
            { return static_cast<ExecType *>(exec_ptr)->try_steal(); };
        }
        m_handle.resume();
        return m_handle.promise().get(interval, std::forward<Callback>(cb));
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
    static std::suspend_never initial_suspend() noexcept
    {
        return {};
    }
    static auto final_suspend() noexcept
    {
        return typename detail::basic_promise_type<T>::final_awaiter{};
    }
    auto get_return_object()
    {
        return task<T>{handle_type::from_promise(*this)};
    }
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
    static std::suspend_never initial_suspend() noexcept
    {
        return {};
    }
    static auto final_suspend() noexcept
    {
        return typename detail::basic_promise_type<void>::final_awaiter{};
    }
    auto get_return_object()
    {
        return task<void>{handle_type::from_promise(*this)};
    }
    void return_void()
    {
        this->set_value();
    }
};

template <typename T> struct lazy_task_promise_type_impl final : detail::basic_promise_type<T>
{
    using handle_type = std::coroutine_handle<lazy_task_promise_type_impl<T>>;
    static std::suspend_always initial_suspend() noexcept
    {
        return {};
    }
    static auto final_suspend() noexcept
    {
        return typename detail::basic_promise_type<T>::final_awaiter{};
    }
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

template <> struct lazy_task_promise_type_impl<void> final : detail::basic_promise_type<void>
{
    using handle_type = std::coroutine_handle<lazy_task_promise_type_impl<void>>;
    static std::suspend_always initial_suspend() noexcept
    {
        return {};
    }
    static auto final_suspend() noexcept
    {
        return typename detail::basic_promise_type<void>::final_awaiter{};
    }
    auto get_return_object()
    {
        return lazy_task<void>{handle_type::from_promise(*this)};
    }
    void return_void()
    {
        this->set_value();
    }
};

} // namespace pot::coroutines

namespace std
{
template <typename T, typename... Args> struct coroutine_traits<pot::coroutines::lazy_task<T>, Args...>
{
    using promise_type = typename pot::coroutines::lazy_task<T>::promise_type;
};

template <typename T, typename... Args> struct coroutine_traits<pot::coroutines::task<T>, Args...>
{
    using promise_type = typename pot::coroutines::task<T>::promise_type;
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
