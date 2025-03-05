#pragma once

#include <coroutine>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <source_location>
#include <chrono>
#include <type_traits>
#include <exception>

#include "pot/tasks/impl/shared_state.h"

namespace pot::coroutines
{
    template <typename T>
    class basic_promise_type
    {
    protected:
        std::coroutine_handle<> m_continuation;
        std::shared_ptr<tasks::details::shared_state<T>> m_state;

    public:
        basic_promise_type() : m_state(std::make_shared<tasks::details::shared_state<T>>()) {}

        void set_continuation(std::coroutine_handle<> continuation) noexcept
        {
            m_continuation = continuation;
        }

        std::coroutine_handle<> continuation() const noexcept
        {
            return m_continuation;
        }

        auto get_shared_state() const noexcept { return m_state; }

        auto operator co_await() noexcept
        {
            return awaiter_t{m_state};
        }

        struct awaiter_t
        {
            std::shared_ptr<tasks::details::shared_state<T>> m_state;
            std::coroutine_handle<> m_continuation;

            bool await_ready() const noexcept
            {
                return m_state->is_ready();
            }

            void await_suspend(std::coroutine_handle<> h) noexcept
            {
                m_continuation = h;
                m_state->set_continuation(h);
            }

            T await_resume()
            {
                if (m_state->has_exception())
                    std::rethrow_exception(m_state->get_exception());

                return m_state->get_value();
            }
        };

        template <typename ValueType>
            requires(!std::is_void_v<T> && std::is_convertible_v<ValueType, T>)
        void set_value(ValueType &&value)
        {
            assert(m_state);
            m_state->set_value(std::forward<ValueType>(value));
        }

        void set_value()
            requires std::is_void_v<T>
        {
            assert(m_state);
            m_state->set_value();
        }

        void set_exception(std::exception_ptr ex)
        {
            assert(m_state);
            m_state->set_exception(ex);
        }

        static constexpr std::suspend_never initial_suspend() noexcept { return {}; }
        static constexpr std::suspend_always final_suspend() noexcept { return {}; }
    };

    template <typename T>
    class [[nodiscard]] task
    {
    public:
        struct promise_type : public basic_promise_type<T>
        {

            task get_return_object()
            {
                return task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            task get_future()
            {
                return get_return_object();
            }

            std::suspend_always yield_value(T value)
            {
                this->m_state->set_value(std::move(value));
                return {};
            }

            auto get_shared_state() const noexcept
            {
                return this->m_state;
            }

            template <typename U>
                requires std::convertible_to<U, T>
            void return_value(U &&value)
            {
                this->m_state->set_value(std::forward<U>(value));
            }

            void unhandled_exception()
            {
                this->m_state->set_exception(std::current_exception());
            }
        };

        using handle_type = std::coroutine_handle<promise_type>;

        explicit task(handle_type h) noexcept : m_handle(h) {}
        task() noexcept : m_handle(nullptr) {}
        task(task &&rhs) noexcept : m_handle(rhs.m_handle)
        {
            rhs.m_handle = nullptr;
        }

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

        bool await_ready() const noexcept
        {
            return m_handle && m_handle.done();
        }

        void await_suspend(std::coroutine_handle<> continuation) noexcept
        {
            if (!m_handle)
            {
                continuation.resume();
                return;
            }

            try
            {
                m_handle.promise().set_continuation(continuation);
                if (!m_handle.done())
                {
                    m_handle.resume();
                }
                else
                {
                    continuation.resume();
                }
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
            {
                throw std::runtime_error("Invalid coroutine handle");
            }
            if constexpr (std::is_void_v<T>)
            {
                m_handle.promise().get_shared_state()->get();
                return;
            }
            else
            {
                return m_handle.promise().get_shared_state()->get();
            }
        }

        T get()
        {
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

        auto operator co_await() noexcept
        {
            struct awaiter
            {
                handle_type m_handle;

                bool await_ready() const noexcept
                {
                    return m_handle || m_handle.done();
                }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept
                {
                    m_handle.promise().set_continuation(continuation);
                    return m_handle;
                }

                T await_resume()
                {
                    return m_handle.promise().get_shared_state()->get();
                }
            };
            return awaiter{m_handle};
        }

    private:
        handle_type m_handle;
    };

    template <>
    struct task<void>::promise_type : basic_promise_type<void>
    {
        task<void> get_return_object()
        {
            return task<void>{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        task get_future()
        {
            return get_return_object();
        }

        void return_void()
        {
            this->m_state->set_value();
        }

        void unhandled_exception()
        {
            this->m_state->set_exception(std::current_exception());
        }
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