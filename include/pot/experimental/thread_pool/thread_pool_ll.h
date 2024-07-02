 #pragma once

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <queue>
#include <future>
#include <unordered_set>
#include <cassert>
#include <functional>
#include <atomic>

namespace pot::experimental
{
    namespace detail
    {
        template <typename Ret>
        class Task
        {
        public:
            template <typename Func, typename... Args>
            Task(Func func, Args&&... args)
            {
                m_task = std::packaged_task<Ret()>([func, args...]() { return func(args...); });
                m_future = m_task.get_future();
            }

            void operator()()
            {
                m_task();
            }

            std::future<Ret> get_future()
            {
                return std::move(m_future);
            }

        private:
            std::packaged_task<Ret()> m_task;
            std::future<Ret> m_future;
        };
    }

    class thread_pool_ll
    {
    public:
        thread_pool_ll(int pool_size) : m_stop(false), m_pool_size(pool_size)
        {
            m_local_queues.resize(pool_size);
            for (int i = 0; i < pool_size; ++i)
            {
                m_threads.emplace_back(&thread_pool_ll::run, this, i);
            }
        }

        ~thread_pool_ll()
        {
            {
                std::unique_lock<std::mutex> lock(m_global_mutex);
                m_stop = true;
            }
            m_global_cv.notify_all();
            for (auto& thread : m_threads)
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }
        }

        template <typename Func, typename... Args>
        auto add_task(Func func, Args&&... args)
        {
            using Ret = decltype(func(args...));
            auto task = std::make_shared<details::Task<Ret>>(func, std::forward<Args>(args)...);
            {
                std::unique_lock<std::mutex> lock(m_global_mutex);
                m_global_queue.push([task]() { (*task)(); });
            }
            m_global_cv.notify_one();

            return task->get_future();
        }

        template <typename Future>
        void wait(Future& future)
        {
            future.wait();
        }

        template <typename ResultType, typename Future>
        ResultType wait_result(Future& future)
        {
            return future.get();
        }

    private:
        void run(int index)
        {
            auto& local_queue = m_local_queues[index];

            while (true)
            {
                std::function<void()> task;

                if (!local_queue.empty())
                {
                    task = std::move(local_queue.front());
                    local_queue.pop();
                }
                else
                {
                    std::unique_lock<std::mutex> lock(m_global_mutex);
                    m_global_cv.wait(lock, [this] { return m_stop || !m_global_queue.empty(); });

                    if (m_stop && m_global_queue.empty())
                        return;

                    if (!m_global_queue.empty())
                    {
                        task = std::move(m_global_queue.front());
                        m_global_queue.pop();
                    }
                    else
                    {
                        for (int i = 0; i < m_pool_size; ++i)
                        {
                            if (i != index && !m_local_queues[i].empty())
                            {
                                task = std::move(m_local_queues[i].front());
                                m_local_queues[i].pop();
                                break;
                            }
                        }
                    }
                }

                if (task)
                {
                    task();
                }
            }
        }

        int m_pool_size;
        std::vector<std::thread> m_threads;
        std::vector<std::queue<std::function<void()>>> m_local_queues;

        std::queue<std::function<void()>> m_global_queue;
        std::mutex m_global_mutex;
        std::condition_variable m_global_cv;

        std::atomic<bool> m_stop;
    };
}
