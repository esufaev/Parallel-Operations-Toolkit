#pragma once

#include <string>
#include <future>
#include <functional>

#include "pot/coroutines/task_coroutine.h"

namespace pot
{
    namespace executors
    {
    }

    class executor
    {
    protected:
        template <typename T>
        using TaskType = pot::coroutines::task<T>;

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

        template <typename Func, typename... Args>
        auto run(Func func, Args... args)
        {
            using return_type = std::invoke_result_t<Func, Args...>;

            if constexpr (std::is_same_v<return_type, TaskType<void>>)
            {
                auto lpromise = std::make_shared<pot::coroutines::promise<void>>();
                pot::coroutines::task<void> task = lpromise->get_task();

                auto lam = [promise = std::move(lpromise), func = std::forward<Func>(func), args...]() mutable -> TaskType<void>
                {
                    try
                    {
                        co_await func(args...);
                        promise->set_value();
                    }
                    catch (...)
                    {
                        promise->set_exception(std::current_exception());
                    }
                    co_return;
                };

                derived_execute([lam = std::move(lam)]() mutable
                                { lam(); });

                return task;
            }
            else
            {
                auto lpromise = std::make_shared<pot::coroutines::promise<return_type>>();
                pot::coroutines::task<return_type> task = lpromise->get_task();
                auto lam = [promise = std::move(lpromise), func = std::forward<Func>(func), args...]() mutable
                {
                    try
                    {
                        if constexpr (std::is_void_v<return_type>)
                        {
                            std::invoke(func, args...);
                            promise->set_value();
                        }
                        else
                        {
                            return_type res = std::invoke(func, args...);
                            promise->set_value(res);
                        }
                    }
                    catch (...)
                    {
                        promise->set_exception(std::current_exception());
                    }
                };

                derived_execute(std::move(lam));

                return task;
            }
        }

        virtual void shutdown() = 0;

        [[nodiscard]] virtual size_t thread_count() const { return 1; }
    };
}
