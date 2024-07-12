#pragma once

#include <iostream>
#include <cassert>
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

namespace pot::experimental::thread_pool
{
    template <bool global_queue_mode>
    class thread_pool_esu
    {
    public:
        class thread_esu;
        struct empty_type
        {
        };

        template <typename TrueType, typename FalseType>
        using mode_type = std::conditional_t<global_queue_mode, TrueType, FalseType>;

        thread_pool_esu(size_t num_threads = std::thread::hardware_concurrency())
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
                for (auto &worker : m_threads)
                {
                    worker->set_other_workers(&m_threads);
                }
            }
        }

        ~thread_pool_esu()
        {
            for (auto &worker : m_threads)
            {
                worker->stop();
            }
            for (auto &worker : m_threads)
            {
                if (worker->joinable())
                {
                    worker->join();
                }
            }
        }

        template <typename Func, typename... Args>
        auto run(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
        {
            using return_type = decltype(func(args...));
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
            std::future<return_type> result = task->get_future();
            if constexpr (global_queue_mode)
            {
                {
                    std::lock_guard lock(m_tasks_mutex);
                    m_tasks.emplace([task]
                                    { (*task)(); });
                }
                m_condition.notify_one();
            }
            else
            {
                m_threads[m_current_thread++ % m_threads.size()]->run_detached([task]
                                                                               { (*task)(); });
            }
            return result;
        }

        template <typename Func, typename... Args>
        void run_detached(Func &&func, Args &&...args)
        {
            if constexpr (global_queue_mode)
            {
                {
                    std::lock_guard lock(m_tasks_mutex);
                    m_tasks.emplace(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
                }
                m_condition.notify_one();
            }
            else
            {
                m_threads[m_current_thread++ % m_threads.size()]->run_detached(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
            }
        }

    private:
        std::vector<std::unique_ptr<thread_esu>> m_threads;

        mode_type<std::mutex, empty_type> m_tasks_mutex;
        mode_type<std::condition_variable, empty_type> m_condition;
        mode_type<std::queue<std::function<void()>>, empty_type> m_tasks;

        mode_type<empty_type, std::atomic_uint64_t> m_current_thread;
    };

    template <bool global_queue_mode>
    class thread_pool_esu<global_queue_mode>::thread_esu
    {
    public:
        explicit thread_esu(std::mutex &m, std::condition_variable &cv,
                            std::queue<std::function<void()>> &tasks,
                            const size_t id = 0, std::string thread_name = {})
            : m_mutex(m), m_condition(cv), m_tasks(tasks), m_id(id),
              m_thread_name(std::move(thread_name)), m_running(true)
        {
            m_thread = std::thread(&thread_esu::thread_loop, this);
        }

        explicit thread_esu(const size_t id = 0, std::string thread_name = {})
            : m_mutex(m_own_mutex), m_condition(m_own_condition), m_tasks(m_own_tasks), m_id(id),
              m_thread_name(std::move(thread_name)), m_running(true)
        {
            m_thread = std::thread(&thread_esu::thread_loop, this);
        }

        ~thread_esu()
        {
            stop();
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

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

        template <typename Func, typename... Args>
        auto run(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
        {
            using return_type = decltype(func(args...));
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
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
        {
            {
                std::lock_guard lock(m_mutex);
                m_tasks.emplace(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
            }
            m_condition.notify_one();
        }

        void set_other_workers(std::vector<std::unique_ptr<thread_esu>> *other_workers)
        {
            m_other_workers = other_workers;
        }

        void stop()
        {
            {
                std::lock_guard lock(m_mutex);
                m_running = false;
            }
            m_condition.notify_all();
        }

    private:
        void thread_loop()
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock lock(m_mutex);
                    m_condition.wait(lock, [&]
                                     { return !m_running || !m_tasks.empty(); });

                    if (!m_running && m_tasks.empty())
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

        std::mutex m_own_mutex;
        std::condition_variable m_own_condition;
        std::queue<std::function<void()>> m_own_tasks;

        size_t m_id;
        std::string m_thread_name;

        std::thread m_thread;
        std::atomic_bool m_running;
        std::vector<std::unique_ptr<thread_esu>> *m_other_workers{nullptr};
    };

    using thread_pool_gq_esu = thread_pool_esu<true>;

    using thread_pool_lq_esu = thread_pool_esu<false>;
}
