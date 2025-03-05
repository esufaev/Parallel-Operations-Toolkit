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
            using lpromise_type = std::conditional_t<pot::traits::concepts::is_task<return_type>,
                                                     typename return_type::promise_type,
                                                     std::promise<return_type>>;
            std::shared_ptr<lpromise_type> lpromise = std::make_shared<lpromise_type>();
            auto future = lpromise->get_future();

            auto lam = [promise = lpromise, func = std::forward<Func>(func), args...]() mutable -> return_type
            {
                try
                {
                    if constexpr (std::is_void_v<pot::traits::task_value_type_t<return_type>>)
                    {
                        if constexpr (pot::traits::concepts::is_task<return_type>)
                            co_await func(args...);
                        else
                            std::invoke(func, args...);
                        promise->set_value();
                    }
                    else
                    {
                        return_type res;
                        if constexpr (pot::traits::concepts::is_task<return_type>)
                            res = co_await func(args...);
                        else
                            res = std::invoke(func, args...);
                        promise->set_value(std::move(res));
                    }
                }
                catch (...)
                {
                    promise->set_exception(std::current_exception());
                }
            };

            derived_execute(std::move(lam));
            return future;
        }

        virtual void shutdown() = 0;

        [[nodiscard]] virtual size_t thread_count() const { return 1; }
    };
}
