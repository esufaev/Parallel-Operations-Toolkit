#pragma once

#include "pot/executor.h"

#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <string>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <iostream>

namespace pot::executors
{
    template <bool global_queue_mode>
    class thread_pool_executor;

    using thread_pool_executor_gq = thread_pool_executor<true>;
    using thread_pool_executor_lq = thread_pool_executor<false>;
}

template <bool global_queue_mode>
class pot::executors::thread_pool_executor : public executor
{
public:
    class thread_esu;
    struct empty_type
    {
    };

    template <typename TrueType, typename FalseType>
    using mode_type = std::conditional_t<global_queue_mode, TrueType, FalseType>;

    explicit thread_pool_executor(std::string name, size_t num_threads = std::thread::hardware_concurrency())
        : executor(std::move(name)), m_shutdown(false)
    {
        m_threads.reserve(num_threads);
        if constexpr (global_queue_mode)
        {
            for (size_t i = 0; i < num_threads; ++i)
            {
                m_threads.push_back(std::make_unique<thread_esu>(
                    m_tasks_mutex, m_condition, m_tasks, i, "Thread " + std::to_string(i)));
            }
        }
        else
        {
            m_current_thread = 0;
            for (size_t i = 0; i < num_threads; ++i)
            {
                m_threads.push_back(std::make_unique<thread_esu>(i, "Thread " + std::to_string(i)));
            }
            for (auto &thread : m_threads)
            {
                thread->set_other_workers(&m_threads);
            }
        }
    }

    ~thread_pool_executor() override { shutdown(); }

    void shutdown() override
    {
        m_shutdown = true;

        for (auto &thread : m_threads)
        {
            thread->request_stop();
        }

        if constexpr (global_queue_mode)
        {
            m_condition.notify_all();
        }
        else
        {
            for (auto &thread : m_threads)
            {
                thread->notify();
            }
        }

        for (auto &thread : m_threads)
        {
            if (thread->joinable())
            {
                thread->join();
            }
        }
    }


    [[nodiscard]] size_t thread_count() const { return m_threads.size(); }

protected:
    void derived_execute(std::function<void()> func) override
    {
        if constexpr (global_queue_mode)
        {
            {
                std::lock_guard lock(m_tasks_mutex);
                m_tasks.emplace(std::move(func));
            }
            m_condition.notify_one();
        }
        else
        {
            m_threads[m_current_thread++ % m_threads.size()]->run_detached(std::move(func));
        }
    }

private:
    std::atomic_bool m_shutdown;
    std::vector<std::unique_ptr<thread_esu>> m_threads;

    mode_type<std::mutex, empty_type> m_tasks_mutex;
    mode_type<std::condition_variable, empty_type> m_condition;
    mode_type<std::queue<std::function<void()>>, empty_type> m_tasks;

    mode_type<empty_type, std::atomic_uint64_t> m_current_thread;
};

template <bool global_queue_mode>
class pot::executors::thread_pool_executor<global_queue_mode>::thread_esu
{
public:
    explicit thread_esu(std::mutex &m, std::condition_variable &cv,
                        std::queue<std::function<void()>> &tasks,
                        const size_t id = 0, std::string thread_name = {})
        requires(global_queue_mode)
        : m_mutex(m), m_condition(cv), m_tasks(tasks), m_id(id),
          m_thread_name(std::move(thread_name)), m_stop_source(std::stop_source())
    {
        m_thread = std::jthread(&thread_esu::thread_loop, this, m_stop_source.get_token());
    }

    explicit thread_esu(const size_t id = 0, std::string thread_name = {})
        requires(!global_queue_mode)
        : m_mutex(m_local_mutex), m_condition(m_local_condition), m_tasks(m_local_tasks), m_id(id),
          m_thread_name(std::move(thread_name)), m_stop_source(std::stop_source())
    {
        m_thread = std::jthread(&thread_esu::thread_loop, this, m_stop_source.get_token());
    }

    ~thread_esu() = default;

    bool joinable() const
    {
        return m_thread.joinable();
    }

    void join()
    {
        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    void request_stop()
    {
        m_stop_source.request_stop();
    }

    void notify()
    {
        m_condition.notify_one();
    }

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

    void set_other_workers(std::vector<std::unique_ptr<thread_esu>> *other_workers)
    {
        m_other_workers = other_workers;
    }

    [[nodiscard]] size_t id() const { return m_id; }
    [[nodiscard]] std::string_view thread_name() const { return m_thread_name; }

private:
    void thread_loop(std::stop_token stop_token)
    {
        while (!stop_token.stop_requested())
        {
            std::function<void()> task;
            {
                std::unique_lock lock(m_mutex);
                m_condition.wait(lock, [&]
                                 { return stop_token.stop_requested() || !m_tasks.empty(); });

                if (stop_token.stop_requested() && m_tasks.empty())
                {
                    return;
                }

                if (!m_tasks.empty())
                {
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
            }

            if (task)
            {
                task();
            }

            if constexpr (!global_queue_mode)
            {
                if (m_tasks.empty() && m_other_workers)
                {
                    task = steal_task();
                    if (task)
                    {
                        task();
                    }
                }
            }
        }
    }

    std::function<void()> steal_task()
    {
        for (auto &worker : *m_other_workers)
        {
            if (worker.get() == this)
            {
                continue;
            }

            std::unique_lock lock(worker->m_mutex);
            if (!worker->m_tasks.empty())
            {
                auto task = std::move(worker->m_tasks.front());
                worker->m_tasks.pop();
                return task;
            }
        }
        return nullptr;
    }

    std::mutex &m_mutex;
    std::condition_variable &m_condition;
    std::queue<std::function<void()>> &m_tasks;

    std::mutex m_local_mutex;
    std::condition_variable m_local_condition;
    std::queue<std::function<void()>> m_local_tasks;

    size_t m_id;
    std::string m_thread_name;

    std::jthread m_thread;
    std::stop_source m_stop_source;
    std::vector<std::unique_ptr<thread_esu>> *m_other_workers{nullptr};
};
