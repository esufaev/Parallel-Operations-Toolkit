#include <atomic>
#include <variant>
#include <coroutine>
#include <stdexcept>
#include <thread>

namespace pot::coroutine::__details
{
    using coro_t = std::coroutine_handle<>;

    template <typename T>
    class basic_promise_type
    {
    public:
        using value_type = T;
        using variant_type = std::conditional_t<std::is_void_v<T>,
                                                std::variant<std::monostate, std::exception_ptr>,
                                                std::variant<std::monostate, T, std::exception_ptr>>;

        basic_promise_type() {}

        template <typename U = T>
            requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
        void set_value(U &&value)
        {
            m_data.template emplace<T>(std::forward<U>(value));
            if (m_ready.exchange(true, std::memory_order_release))
                throw std::runtime_error("Value already set in promise_type.");
            if (m_continuation)
                m_continuation.resume();
            m_continuation = nullptr;
        }

        void set_value()
            requires std::is_void_v<T>
        {
            m_data = std::monostate{};
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Value already set in promise_type.");
            }
            if (m_continuation)
                m_continuation.resume();
            m_continuation = nullptr;
        }

        void set_exception(std::exception_ptr ex)
        {
            m_data = ex;
            if (m_ready.exchange(true, std::memory_order_release))
            {
                throw std::runtime_error("Exception already set in promise_type.");
            }
            if (m_continuation)
                m_continuation.resume();
            m_continuation = nullptr;
        }

        bool is_ready() const noexcept { return m_ready.load(std::memory_order_acquire); }

        void set_continuation(coro_t continuation)
        {
            if (m_ready.load(std::memory_order_acquire)) { continuation.resume(); }
            else
            {
                if (m_continuation) throw std::runtime_error("Continuation already set.");
                m_continuation = continuation;
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
                return std::get<T>(m_data);
        }

        void wait() const
        {
            while (!m_ready.load(std::memory_order_acquire))
                std::this_thread::yield();
        }

        static constexpr std::suspend_never initial_suspend() noexcept { return {}; }
        static constexpr std::suspend_always final_suspend() noexcept { return {}; }

    private:
        std::atomic<bool> m_ready{false};
        variant_type m_data{std::monostate{}};
        coro_t m_continuation;
    };
}