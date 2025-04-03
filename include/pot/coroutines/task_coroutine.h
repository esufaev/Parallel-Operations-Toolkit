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
#include <vector>
#include <variant>
#include <atomic>

namespace pot::coroutines
{
    template <typename T>
    class basic_promise_type
    {
    public:
        using value_type = T;
        using variant_type = std::conditional_t<std::is_void_v<T>,
                                                std::variant<std::monostate, std::exception_ptr>,
                                                std::variant<std::monostate, T, std::exception_ptr>>;

        basic_promise_type() {}

        // auto operator co_await() noexcept
        // {
        //     return awaiter_t{m_state};
        // }

        // struct awaiter_t
        // {
        //     std::shared_ptr<tasks::details::shared_state<T>> m_state;
        //     std::coroutine_handle<> m_continuation;

        //     bool await_ready() const noexcept
        //     {
        //         return m_state->is_ready();
        //     }

        //     void await_suspend(std::coroutine_handle<> h) noexcept
        //     {
        //         m_continuation = h;
        //         m_state->set_continuation(h);
        //     }

        //     T await_resume()
        //     {
        //         if (m_state->has_exception())
        //             std::rethrow_exception(m_state->get_exception());

        //         return m_state->get_value();
        //     }
        // };

        template <typename U = T>
            requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
        void set_value(U &&value)
        {
            m_data.template emplace<T>(std::forward<U>(value));
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Value already set in promise_type.");
            }
            for (auto &cont : m_continuations)
            {
                cont.resume();
            }
            m_continuations.clear();
        }

        void set_value()
            requires std::is_void_v<T>
        {
            m_data = std::monostate{};
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Value already set in promise_type.");
            }
            for (auto &cont : m_continuations)
            {
                cont.resume();
            }
            m_continuations.clear();
        }

        void set_exception(std::exception_ptr ex)
        {
            m_data = ex;
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Exception already set in promise_type.");
            }
            for (auto &cont : m_continuations)
            {
                cont.resume();
            }
            m_continuations.clear();
        }

        bool is_ready() const noexcept
        {
            return m_ready.load(std::memory_order_acquire);
        }

        void set_continuation(std::coroutine_handle<> continuation)
        {
            if (m_ready.load(std::memory_order_acquire))
            {
                continuation.resume();
            }
            else
            {
                m_continuations.push_back(continuation);
            }
        }

        T get()
        {
            wait();
            if (std::holds_alternative<std::exception_ptr>(m_data))
            {
                std::rethrow_exception(std::get<std::exception_ptr>(m_data));
            }
            if constexpr (!std::is_void_v<T>)
            {
                return std::get<T>(m_data);
            }
        }

        void wait() const
        {
            while (!m_ready.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        }

        static constexpr std::suspend_never initial_suspend() noexcept { return {}; }
        static constexpr std::suspend_always final_suspend() noexcept { return {}; }

    private:
        std::atomic<bool> m_ready{false};
        variant_type m_data{std::monostate{}};
        std::vector<std::coroutine_handle<>> m_continuations;
    };

    template <typename T>
    class [[nodiscard]] task
    {
    public:
        struct promise_type : public basic_promise_type<T>
        {

            task<T> get_return_object()
            {
                return task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            task<T> get_future()
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
                this->set_value(std::forward<U>(value));
            }

            void unhandled_exception()
            {
                this->set_exception(std::current_exception());
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
            return m_handle && m_handle.promise().is_ready();
        }

        void await_suspend(std::coroutine_handle<> continuation) noexcept
        {
            // if (!m_handle)
            // {
            //     continuation.resume();
            //     return;
            // }
            // m_handle.promise().get_shared_state()->set_continuation(continuation);

            // try
            // {
            //     if (m_handle && !m_handle.done())
            //     {
            //         m_handle.resume();
            //     }
            //     else
            //     {
            //         continuation.resume();
            //     }
            // }
            // catch (...)
            // {
            //     m_handle.promise().set_exception(std::current_exception());
            //     continuation.resume();
            // }

            m_handle.promise().set_continuation(continuation);
        }

        T await_resume()
        {
            if (!m_handle)
            {
                throw std::runtime_error("Invalid coroutine handle");
            }
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

        // auto operator co_await() noexcept
        // {
        //     struct awaiter
        //     {
        //         handle_type m_handle;

        //         bool await_ready() const noexcept
        //         {
        //             return m_handle || m_handle.done();
        //         }

        //         std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept
        //         {
        //             return m_handle;
        //         }

        //         T await_resume()
        //         {
        //             return m_handle.promise().get_shared_state()->get();
        //         }
        //     };
        //     return awaiter{m_handle};
        // }

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

        task<void> get_future()
        {
            return get_return_object();
        }

        void return_void()
        {
            this->set_value();
        }

        void unhandled_exception()
        {
            this->set_exception(std::current_exception());
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