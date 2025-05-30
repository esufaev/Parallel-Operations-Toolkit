#pragma once

#include <string>
#include <functional>

#include "pot/coroutines/task.h"

namespace pot
{
    namespace executors
    {
    }

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
                            { func(args...); });
        }

        template<typename Func, typename... Args>
        auto run(Func func, Args... args) -> pot::coroutines::task<pot::traits::awaitable_value_type_t<std::invoke_result_t<Func, Args...>>>
        {
            return do_run<false>(std::move(func), std::move(args)...);
        }

        template <typename Func, typename... Args>
        auto lazy_run(Func func, Args... args) -> pot::coroutines::lazy_task<pot::traits::awaitable_value_type_t<std::invoke_result_t<Func, Args...>>>
        {
            return do_run<true>(std::move(func), std::move(args)...);
        }

        virtual void shutdown() = 0;

        [[nodiscard]] virtual size_t thread_count() const { return 1; }

    private:
        struct derived_execute_awaiter
        {
            std::coroutine_handle<> handle;

            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) noexcept { handle = h; }
            void await_resume() const noexcept {}

            void resume() const
            {
                if (handle)
                    handle.resume();
            }
        };

        template <bool Lazy, typename Func, typename... Args>
        auto do_run(Func &&func, Args &&...args) -> std::conditional_t<Lazy, pot::coroutines::lazy_task<pot::traits::awaitable_value_type_t<std::invoke_result_t<Func, Args...>>>,
                                                                       pot::coroutines::task<pot::traits::awaitable_value_type_t<std::invoke_result_t<Func, Args...>>>>
        {
            using return_type = std::invoke_result_t<Func, Args...>;
            auto awaiter = std::make_shared<derived_execute_awaiter>();
            if constexpr (std::is_void_v<typename pot::traits::task_value_type_t<return_type>>)
            {
                derived_execute([func = std::forward<Func>(func), ... args = std::forward<Args>(args), awaiter]() -> pot::coroutines::task<void>
                {
                    if constexpr (pot::traits::concepts::is_task<return_type>)  { co_await std::invoke(std::move(func), std::move(args)...); }
                    else                                                        { std::invoke(std::move(func), std::move(args)...);          }
                    awaiter->resume();
                    co_return; 
                });
                co_await *awaiter;
                co_return;
            }
            else
            {
                auto result = std::make_shared<pot::traits::task_value_type_t<return_type>>();
                derived_execute([func = std::forward<Func>(func), ... args = std::forward<Args>(args), awaiter, result]() -> pot::coroutines::task<void>
                {
                    if constexpr (pot::traits::concepts::is_task<return_type>) { *result = co_await std::invoke(std::move(func), std::move(args)...); }
                    else                                                       { *result = std::invoke(std::move(func), std::move(args)...);          }
                    awaiter->resume();
                    co_return;
                });
                co_await *awaiter;
                co_return *result;
            }
        }
    };
}