module;

#if defined(POT_COMPILER_MSVC)

import <string>;
import <functional>;
import <utility>;
import <type_traits>;
import <coroutine>;

#else

#include <string>
#include <functional>
#include <utility>
#include <type_traits>
#include <coroutine>

#endif


export module pot.executors.executor;

import pot.coroutines.task;

export namespace pot
{
    class executor
    {
    protected:
        std::string m_name;

        virtual void derived_execute(std::function<void()> func) = 0;

    public:
        explicit executor(std::string name) : m_name(std::move(name)) {}
        virtual ~executor() = default;

        executor(const executor &) = delete;
        executor &operator=(const executor &) = delete;

        executor(executor &&) = default;
        executor &operator=(executor &&) = default;

        [[nodiscard]] std::string name() const { return m_name; }

        template <typename Func, typename... Args>
            requires std::is_invocable_v<Func, Args...>
        void run_detached(Func func, Args... args)
        {
            derived_execute([=]()
                            { std::invoke(func, args...); });
        }

        template <typename Func, typename... Args>
        auto run(Func&& func, Args&&... args)
            -> coroutines::task<pot::traits::awaitable_value_t<std::invoke_result_t<Func, Args...>>>
        {
            return do_run<false>(std::forward<Func>(func), std::forward<Args>(args)...);
        }

        template <typename Func, typename... Args>
        auto lazy_run(Func&& func, Args&&... args)
            -> coroutines::lazy_task<pot::traits::awaitable_value_t<std::invoke_result_t<Func, Args...>>>
        {
            return do_run<true>(std::forward<Func>(func), std::forward<Args>(args)...);
        }

        virtual void shutdown() = 0;
        [[nodiscard]] virtual size_t thread_count() const { return 1; }

    private:
        struct switch_awaitable
        {
            executor &ex;
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) const
            {
                ex.derived_execute([h]() mutable
                                   { h.resume(); });
            }
            void await_resume() const noexcept {}
        };

    private:
        template <bool Lazy, typename Func, typename... Args>
        auto do_run(Func&& func, Args&&... args)
            -> std::conditional_t<
                Lazy,
                coroutines::lazy_task<pot::traits::awaitable_value_t<std::invoke_result_t<Func, Args...>>>,
                coroutines::task<pot::traits::awaitable_value_t<std::invoke_result_t<Func, Args...>>>>
        {
            using Ret = std::invoke_result_t<Func, Args...>;
            using Val = pot::traits::awaitable_value_t<Ret>;

            auto f = std::decay_t<Func>(std::forward<Func>(func));
            auto ar = std::make_tuple(std::decay_t<Args>(std::forward<Args>(args))...);

            co_await switch_awaitable{*this};

            if constexpr (std::is_void_v<Val>)
            {
                if constexpr (pot::traits::is_task_v<Ret> || pot::traits::is_lazy_task_v<Ret>)
                {
                    co_await std::apply(f, ar);
                    co_return;
                }
                else
                {
                    std::apply(f, ar);
                    co_return;
                }
            }
            else
            {
                if constexpr (pot::traits::is_task_v<Ret> || pot::traits::is_lazy_task_v<Ret>)
                {
                    co_return co_await std::apply(f, ar);
                }
                else
                {
                    co_return std::apply(f, ar);
                }
            }
        }
    };
} // namespace pot
