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
            constexpr bool is_coroutine = pot::traits::concepts::is_task<return_type>;

            using promise_type = std::conditional_t<is_coroutine,
                                                    typename return_type::promise_type,
                                                    std::promise<return_type>>;
            using future_type = std::conditional_t<is_coroutine,
                                                   return_type,
                                                   std::future<return_type>>;
            using value_type = std::conditional_t<is_coroutine,
                                                  pot::traits::task_value_type_t<return_type>,
                                                  return_type>;

            auto lpromise = std::make_shared<promise_type>();
            future_type future = lpromise->get_future();

            auto lam = [lpromise, func = std::forward<Func>(func), args...]() mutable -> return_type
            {
                try
                {
                    if constexpr (is_coroutine)
                    {
                        if constexpr (std::is_void_v<value_type>)
                        {
                            co_await func(args...);
                            lpromise->set_value();
                        }
                        else
                        {
                            value_type result = co_await func(args...);
                            lpromise->set_value(std::move(result));
                        }
                    }
                    else
                    {
                        if constexpr (std::is_void_v<value_type>)
                        {
                            std::invoke(func, args...);
                            lpromise->set_value();
                        }
                        else
                        {
                            value_type res = std::invoke(func, args...);
                            lpromise->set_value(std::move(res));
                        }
                    }
                }
                catch (...)
                {
                    lpromise->set_exception(std::current_exception());
                }
            };

            derived_execute(std::move(lam));
            return future;
        }

        virtual void shutdown() = 0;

        [[nodiscard]] virtual size_t thread_count() const { return 1; }
    };
}