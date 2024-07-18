#pragma once

#include <string>
#include <future>
#include <functional>


namespace pot
{
    namespace executors {}

    class executor
    {
    protected:
        template<typename T>
        using FutureType = std::future<T>;

        std::string m_name;

        virtual void derived_execute(std::function<void()> func) = 0;

    public:
        explicit executor(std::string name) : m_name(std::move(name)) {}
        virtual ~executor() = default;

        executor(const executor&) = delete;
        executor& operator=(const executor&) = delete;
        executor(executor&&) = default;
        executor& operator=(executor&&) = default;

        [[nodiscard]] std::string name() const { return m_name; }

        template<typename Func, typename...Args> requires std::is_invocable_v<Func, Args...>
        void run_detached(Func func, Args...args)
        {
            derived_execute([=]() { func(args...); });
        }

        template<typename Func, typename...Args> requires std::is_invocable_v<Func, Args...>
        auto run(Func func, Args...args) -> FutureType<std::invoke_result_t<Func, Args...>>
        {
            using return_type = std::invoke_result_t<Func, Args...>;

            auto lpromise = std::make_shared<std::promise<return_type>>();
            std::future<return_type> future = lpromise->get_future();
            auto lam = [promise = std::move(lpromise), func = std::forward<Func>(func), args...]() mutable {
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


            // std::packaged_task<return_type()> task([func, args...]() -> return_type { return func(args...); });
            // auto future = task.get_future();

            derived_execute(std::move(lam));

            return future;
        }


        virtual void shutdown() = 0;

    };
}