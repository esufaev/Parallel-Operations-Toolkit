#pragma once

#include <atomic>
#include <exception>
#include <chrono>
#include <variant>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <type_traits>

namespace pot::tasks::details
{

    template <typename T>
    class shared_state
    {
    public:
        using value_type = T;
        using variant_type = std::conditional_t<std::is_void_v<T>,
                                                std::variant<std::monostate, std::exception_ptr>,
                                                std::variant<std::monostate, T, std::exception_ptr>>;

        shared_state() = default;

        shared_state(const shared_state &) = delete;
        shared_state &operator=(const shared_state &) = delete;

        template <typename U = T>
        void set_value(U &&value)
            requires(!std::is_void_v<T>)
        {
            m_data.template emplace<T>(std::forward<U>(value));
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Value already set in shared_state.");
            }
            notify_continuation();
        }

        void set_value()
            requires(std::is_void_v<T>)
        {
            m_data = std::monostate{};
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Value already set in shared_state.");
            }
            notify_continuation();
        }

        void set_exception(std::exception_ptr ex)
        {
            m_data = ex;
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Exception already set in shared_state.");
            }
            notify_continuation();
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

        void set_continuation(std::coroutine_handle<> continuation) noexcept
        {
            m_continuation = continuation;
        }

        void wait() const
        {
            while (!m_ready.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        }

    private:
        void notify_continuation()
        {
            if (m_continuation)
            {
                m_continuation.resume();
            }
        }

        std::atomic<bool> m_ready{false};
        variant_type m_data{std::monostate{}};
        std::coroutine_handle<> m_continuation{nullptr};
    };

} // namespace pot::tasks::details