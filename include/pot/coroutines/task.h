#pragma once

#include "pot/coroutines/__details/basic_promise_type.h"

namespace pot::coroutines
{
    using coro_t = std::coroutine_handle<>;

    template <typename T>
    class [[nodiscard]] task
    {
    public:
        struct promise_type : public pot::coroutine::__details::basic_promise_type<T>
        {
            task<T> get_return_object() { return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
            task<T> get_future() { return get_return_object(); }

            std::suspend_always yield_value(T value)
            {
                this->m_state->set_value(std::move(value));
                return {};
            }

            template <typename U>
                requires std::convertible_to<U, T>
            void return_value(U &&value)
            {
                this->set_value(std::forward<U>(value));
            }

            void unhandled_exception() { this->set_exception(std::current_exception()); }
        };

        using handle_type = std::coroutine_handle<promise_type>;

        explicit task(handle_type h) noexcept : m_handle(h) {}
        task() noexcept : m_handle(nullptr) {}

        task(task &&rhs) noexcept : m_handle(rhs.m_handle) { rhs.m_handle = nullptr; }
        task &operator=(task &&rhs) noexcept
        {
            if (this != &rhs)
            {
                if (m_handle)
                {
                    m_handle.destroy();
                }
                m_handle = rhs.m_handle;
                rhs.m_handle = nullptr;
            }
            return *this;
        }

        task(const task &) = delete;
        task &operator=(const task &) = delete;

        ~task()
        {
            if (m_handle && m_handle.done())
            {
                m_handle.destroy();
            }
        }

        bool await_ready() const noexcept { return m_handle && m_handle.promise().is_ready(); }
        void await_suspend(coro_t continuation) noexcept { m_handle.promise().set_continuation(continuation); }
        T await_resume()
        {
            if (!m_handle)
                throw std::runtime_error("Invalid coroutine handle");

            if constexpr (std::is_void_v<T>) m_handle.promise().get();
            else return m_handle.promise().get();
        }

        T get() { return await_resume(); }

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
    struct task<void>::promise_type : pot::coroutine::__details::basic_promise_type<void>
    {
        task<void> get_return_object() { return task<void>{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        task<void> get_future() { return get_return_object(); }

        void return_void() { set_value(); }
        void unhandled_exception() { set_exception(std::current_exception()); }
    };
}

namespace pot::traits
{
    template <typename T>
    struct task_value_type;

    template <typename ValueType>
    struct task_value_type<coroutines::task<ValueType>>
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

    template <template <typename> typename Task, typename T>
    struct awaitable_value_type<Task<T>>
    {
        using type = T;
    };

    template <typename T>
    using awaitable_value_type_t = typename awaitable_value_type<T>::type;

    namespace concepts
    {
        template <typename Type>
        concept is_task = requires(Type t) {
            typename task_value_type_t<std::remove_cvref_t<Type>>;
        };
    }
}