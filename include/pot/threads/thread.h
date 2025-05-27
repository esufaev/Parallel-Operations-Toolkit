#pragma once

#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <future>

namespace pot
{
    class thread
    {
    public:
        thread(size_t id, std::string thread_name)
            : m_id(id), m_thread_name(std::move(thread_name)) {}
        virtual ~thread() = default;

        virtual bool joinable() const = 0;
        virtual void join() = 0;
        virtual void request_stop() = 0;
        virtual void notify() = 0;

        template <typename Func, typename... Args>
        auto run(Func &&func, Args &&...args) -> std::invoke_result_t<Func, Args...>
            requires std::is_invocable_v<Func, Args...>
        {
            using return_type = decltype(func(args...));
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::function<return_type()>(std::forward<Func>(func), std::forward<Args>(args)...));
            std::future<return_type> result = task->get_future();
            {
                std::lock_guard lock(m_mutex);
                m_tasks.emplace([task]
                                { (*task)(); });
            }
            m_condition.notify_one();
            return result;
        }

        template <typename Func, typename... Args>
        void run_detached(Func &&func, Args &&...args)
            requires std::is_invocable_v<Func, Args...>
        {
            {
                std::lock_guard lock(m_mutex);
                m_tasks.emplace(std::function<void()>(std::forward<Func>(func), std::forward<Args>(args)...));
            }
            m_condition.notify_one();
        }

        [[nodiscard]] size_t id() const { return m_id; }
        [[nodiscard]] std::string_view thread_name() const { return m_thread_name; }

    protected:
        std::mutex m_mutex;
        std::condition_variable m_condition;
        std::queue<std::function<void()>> m_tasks;

        size_t m_id;
        std::string m_thread_name;

        std::jthread m_thread;
        std::stop_source m_stop_source;
    };
}