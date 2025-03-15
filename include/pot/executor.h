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
            if constexpr (pot::traits::concepts::is_task<return_type>)
            {
                using task_type = std::remove_cvref_t<return_type>;
                using promise_type = typename task_type::promise_type;

                auto lpromise = std::make_shared<promise_type>();
                auto task = lpromise->get_return_object();
                auto future = [promise = lpromise, func = std::forward<Func>(func), args...]() mutable -> return_type
                {
                    try
                    {
                        if constexpr (std::is_void_v<pot::traits::task_value_type_t<task_type>>)
                        {
                            co_await func(args...);
                            promise->set_value();
                        }
                        else
                        {
                            auto result = co_await func(args...);
                            promise->set_value(std::move(result));
                        }
                    }
                    catch (...)
                    {
                        promise->set_exception(std::current_exception());
                    }
                };

                derived_execute(std::move(future));
                return task;
            }
            else
            {
                auto lpromise = std::make_shared<std::promise<return_type>>();
                std::future<return_type> future = lpromise->get_future();
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

                derived_execute(lam);

                return future;
            }
        }

        virtual void shutdown() = 0;

        [[nodiscard]] virtual size_t thread_count() const { return 1; }
    };
}