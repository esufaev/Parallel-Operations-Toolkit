#pragma once

#include <coroutine>
#include <memory>
#include <stdexcept>
#include <utility>
#include <source_location>
#include <type_traits>
#include <exception>

#include "pot/tasks/impl/shared_state.h"

namespace pot::coroutines
{
    /*template <typename ValueType>
    class [[nodiscard]] task
    {
    public:
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        explicit task(std::shared_ptr<pot::tasks::details::shared_state<ValueType>> state)
            : m_state(std::move(state)), m_handle(nullptr)
        {
            m_handle = std::coroutine_handle<promise_type>::from_address(this);
        }

        explicit task(handle_type h) noexcept
            : m_handle(h) {}

        explicit task(handle_type h, std::shared_ptr<pot::tasks::details::shared_state<ValueType>> state) noexcept
            : m_state(std::move(state)), m_handle(h)
        {
            h.promise().m_state = m_state;
        }

        task(task &&rhs) noexcept
            : m_state(std::move(rhs.m_state)), m_handle(std::exchange(rhs.m_handle, nullptr))
        {
        }

        task &operator=(task &&rhs) noexcept
        {
            if (this != &rhs)
            {
                if (m_handle && m_handle.done())
                    m_handle.destroy();
                m_handle = std::exchange(rhs.m_handle, nullptr);
                m_state = std::move(rhs.m_state);
            }
            return *this;
        }

        task(const task &) = delete;
        task &operator=(const task &) = delete;

        ~task()
        {
            if (m_handle && m_handle.done())
                m_handle.destroy();
        }

        void wait()
        {
            ensure_handle();
            m_handle.promise().m_state->wait();
        }

        ValueType get()
        {
            ensure_handle();
            if constexpr (std::is_void_v<ValueType>)
            {
                m_handle.promise().m_state->get();
                return;
            }
            else
                return m_handle.promise().m_state->get();
        }

        [[nodiscard]] bool is_ready() const noexcept
        {
            return m_state && m_state->is_ready();
        }

        [[nodiscard]] bool await_ready() const noexcept
        {
            return is_ready();
        }

        void await_suspend(std::coroutine_handle<> h) const
        {
            h.resume();
        }

        ValueType await_resume()
        {
            if constexpr (std::is_void_v<ValueType>)
            {
                get();
                return;
            }
            else
                return get();
        }

        ValueType next()
        {
            ensure_handle();
            if (m_handle && !m_handle.done())
            {
                m_handle.resume();
                return m_handle.promise().m_state->get_value();
            }
            throw std::runtime_error(std::string(std::source_location::current().function_name()) + " - Task completed, no more values.");
        }

    private:
        std::shared_ptr<tasks::details::shared_state<ValueType>> m_state;
        handle_type m_handle;

        void ensure_handle(const std::source_location& location = std::source_location::current()) const
        {
            assert(m_handle);
            if (!m_handle)
                throw std::runtime_error(std::string(location.function_name()) + " - Attempted to use an empty task.");
        }
    };

    template <typename T>
    struct [[nodiscard]] task<T>::promise_type
    {
        std::shared_ptr<tasks::details::shared_state<T>> m_state;

        promise_type() : m_state(std::make_shared<tasks::details::shared_state<T>>()) {}

        task get_return_object()
        {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        [[nodiscard]] std::suspend_never initial_suspend() const noexcept { return {}; }
        [[nodiscard]] std::suspend_always final_suspend() const noexcept { return {}; }

        void return_value(T value)
        {
            m_state->set_value(std::move(value));
        }

        void unhandled_exception()
        {
            m_state->set_exception(std::current_exception());
        }
    };

    template <>
    struct [[nodiscard]] task<void>::promise_type
    {
        std::shared_ptr<pot::tasks::details::shared_state<void>> m_state;

        promise_type() : m_state(std::make_shared<pot::tasks::details::shared_state<void>>()) {}

        task<void> get_return_object()
        {
            return task<void>{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        [[nodiscard]] std::suspend_never initial_suspend() const noexcept { return {}; }
        [[nodiscard]] std::suspend_always final_suspend() const noexcept { return {}; }

        void return_void()
        {
            m_state->set_value();
        }

        void unhandled_exception()
        {
            m_state->set_exception(std::current_exception());
        }
    };

    template <typename T>
    class [[nodiscard]] promise
    {
    public:
        promise() : m_state(std::make_shared<pot::tasks::details::shared_state<T>>()) {}

        template<typename ValueType>
        requires !std::is_void_v<T> && std::is_convertible_v<ValueType, T>
        void set_value(ValueType&& value)
        {
            ensure_state();
            m_state->set_value(std::forward<ValueType>(value));
        }

        void set_value() requires std::is_void_v<T>
        {
            ensure_state();
            m_state->set_value();
        }

        void set_exception(std::exception_ptr ex)
        {
            ensure_state();
            m_state->set_exception(ex);
        }

        task<T> get_task()
        {
            return task<T>{m_state};
        }

    private:
        std::shared_ptr<pot::tasks::details::shared_state<T>> m_state;

        void ensure_state() const
        {
            if (!m_state)
            {
                throw std::runtime_error("promise: operation on an uninitialized state");
            }
        }
    };*/


    // Добавляем новый базовый класс для общего состояния
template <typename T>
class basic_promise_type {
protected:
    std::shared_ptr<tasks::details::shared_state<T>> m_state;
    std::coroutine_handle<> m_continuation;

public:
    basic_promise_type() : m_state(std::make_shared<tasks::details::shared_state<T>>()) {}

    void set_continuation(std::coroutine_handle<> handle) noexcept {
        m_continuation = handle;
    }

    std::coroutine_handle<> continuation() const noexcept {
        return m_continuation;
    }

    // C++23: добавляем поддержку operator co_await
    auto operator co_await() noexcept {
        return *this;
    }

    // C++23: добавляем поддержку awaiter_t
    struct awaiter_t {
        bool await_ready() const noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
            return h;
        }
        void await_resume() const noexcept {}
    };

    // C++23: используем std::suspend_always вместо custom suspension points
    static constexpr std::suspend_never initial_suspend() noexcept { return {}; }
    static constexpr std::suspend_always final_suspend() noexcept { return {}; }

    auto get_shared_state() const noexcept { return m_state; }

    // template<typename ValueType>
    //     requires !std::is_void_v<T>&& std::is_convertible_v<ValueType, T>
    // void set_value(ValueType&& value)
    // {
    //     assert(m_state);
    //     m_state->set_value(std::forward<ValueType>(value));
    // }
    //
    // void set_value() requires std::is_void_v<T>
    // {
    //     assert(m_state);
    //     m_state->set_value();
    // }
    //
    void set_exception(std::exception_ptr ex)
    {
        assert(m_state);
        m_state->set_exception(ex);
    }
};

template <typename ValueType>
class [[nodiscard]] task {
public:
    struct promise_type : basic_promise_type<ValueType> {
        task get_return_object() {
            return task{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }

        // C++23: добавляем поддержку yield_value для генераторов
        std::suspend_always yield_value(ValueType value) {
            this->m_state->set_value(std::move(value));
            return {};
        }

        template<typename U>
            requires std::convertible_to<U, ValueType>
        void return_value(U&& value) {
            this->m_state->set_value(std::forward<U>(value));
        }

        void unhandled_exception() {
            this->m_state->set_exception(std::current_exception());
        }

        ValueType& result()
        {
            return this->m_state->template get<ValueType>();

        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    // Конструкторы остаются теми же
    explicit task(handle_type h) noexcept : m_handle(h) {}

    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task(task&& rhs) noexcept = default;
    task& operator=(task&& rhs) noexcept = default;

    ~task() {
        if (m_handle && m_handle.done()) {
            m_handle.destroy();
        }
    }

    bool await_ready() const noexcept {
        return m_handle && m_handle.done();
    }

    void await_suspend(std::coroutine_handle<> continuation) noexcept {
        if (!m_handle) {
            // Если handle невалидный, сразу возобновляем continuation
            continuation.resume();
            return;
        }

        try {
            m_handle.promise().set_continuation(continuation);
            // Проверяем состояние перед resume
            if (!m_handle.done()) {
                m_handle.resume();
            }
            else {
                // Если корутина уже завершена, возобновляем continuation
                continuation.resume();
            }
        }
        catch (...) {
            // В случае исключения, сохраняем его и возобновляем continuation
            m_handle.promise().set_exception(std::current_exception());
            continuation.resume();
        }
    }

    ValueType await_resume() {
        if (!m_handle) {
            throw std::runtime_error("Invalid coroutine handle");
        }
        if constexpr (std::is_void_v<ValueType>) {
            m_handle.promise().get_shared_state()->get();
            return;
        }
        else {
            return m_handle.promise().get_shared_state()->get();
        }
    }

    // C++23: добавляем поддержку structured bindings
    // auto operator co_await() const& noexcept {
    //     struct awaiter {
    //         handle_type m_handle;
    //
    //         bool await_ready() const noexcept {
    //             return !m_handle || m_handle.done();
    //         }
    //
    //         std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
    //             m_handle.promise().set_continuation(continuation);
    //             return m_handle;
    //         }
    //
    //         ValueType await_resume() {
    //             return m_handle.promise().get_result();
    //         }
    //     };
    //     return awaiter{ m_handle };
    // }

    ValueType get() {
        return await_resume();
    }

    class iterator {
        handle_type m_handle;
    public:
        using value_type = ValueType;
        using difference_type = std::ptrdiff_t;

        iterator(handle_type h = nullptr) : m_handle(h) {}

        value_type operator*() const { return m_handle.promise().current_value(); }
        iterator& operator++() {
            m_handle.resume();
            return *this;
        }
        bool operator==(const iterator&) const = default;
    };

    auto begin() { return iterator(m_handle); }
    auto end() { return iterator(); }

private:
    handle_type m_handle;
};

// Специализация для void
template <>
struct task<void>::promise_type : basic_promise_type<void> {
    task<void> get_return_object() {
        return task<void>{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    void return_void() {
        this->m_state->set_value();
    }

    void unhandled_exception() {
        this->m_state->set_exception(std::current_exception());
    }
};

} // namespace pot::coroutines




