#pragma once

#include <iostream>
#include <queue>
#include <future>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace pot::experimental
{
    namespace details
    {
        class worker_thread
        {
        public:
            worker_thread() : m_running(true)
            {
                m_worker_thread = std::thread(&worker_thread::run, this);
            }

            ~worker_thread()
            {
                stop();
                if (m_worker_thread.joinable())
                {
                    m_worker_thread.join();
                }
            }

            template <typename Func>
            auto add_task_thread(Func &&func)
            {
                using Ret = decltype(func());
                auto task = std::make_shared<std::packaged_task<Ret()>>(std::forward<Func>(func));
                auto future = task->get_future();
                {
                    std::unique_lock<std::mutex> lock(m_queue_mutex);
                    m_task_queue.push([task]()
                                      { (*task)(); });
                    m_condition.notify_one();
                }
                return future;
            }

            void stop()
            {
                {
                    std::unique_lock<std::mutex> lock(m_queue_mutex);
                    m_running = false;
                }
                m_condition.notify_all();
            }

            size_t get_queue_size()
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                return m_task_queue.size();
            }

            void set_other_workers(std::vector<worker_thread *> *other_workers)
            {
                m_other_workers = other_workers;
            }

        private:
            void run()
            {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(m_queue_mutex);
                        m_condition.wait(lock, [this]()
                                         { return !m_task_queue.empty() || !m_running; });

                        if (!m_running && m_task_queue.empty())
                            return;

                        if (!m_task_queue.empty())
                        {
                            task = std::move(m_task_queue.front());
                            m_task_queue.pop();
                        }
                    }

                    task();

                    if (m_task_queue.empty())
                    {
                        auto task = steal_task();
                        if (task)
                        {
                            task();
                        }
                    }
                }
            }

            std::function<void()> steal_task()
            {
                for (auto worker : *m_other_workers)
                {
                    if (worker == this)
                        continue;

                    std::unique_lock<std::mutex> lock(worker->m_queue_mutex);
                    if (!worker->m_task_queue.empty())
                    {
                        auto task = std::move(worker->m_task_queue.front());
                        worker->m_task_queue.pop();
                        return task;
                    }
                }
                return nullptr;
            }

            std::queue<std::function<void()>> m_task_queue;
            std::mutex m_queue_mutex;
            std::condition_variable m_condition;

            std::thread m_worker_thread;
            std::atomic<bool> m_running;
            std::vector<worker_thread *> *m_other_workers;
        };
    } // namespace details

    namespace thread_pool
    {
        using worker_thread = details::worker_thread;

        class thread_pool_lq_esu
        {
        public:
            explicit thread_pool_lq_esu(size_t numThreads) : m_next_worker(0)
            {
                m_workers.reserve(numThreads);
                for (size_t i = 0; i < numThreads; i++)
                {
                    m_workers.push_back(std::make_unique<worker_thread>());
                }

                for (auto &worker : m_workers)
                {
                    worker->set_other_workers(&m_raw_workers);
                    m_raw_workers.push_back(worker.get());
                }
            }

            thread_pool_lq_esu(const thread_pool_lq_esu &) = delete;
            thread_pool_lq_esu &operator=(const thread_pool_lq_esu &) = delete;
            thread_pool_lq_esu(thread_pool_lq_esu &&other) = delete;
            thread_pool_lq_esu &operator=(thread_pool_lq_esu &&other) = delete;

            ~thread_pool_lq_esu()
            {
                for (auto &worker : m_workers)
                {
                    worker->stop();
                }
            }

            template <typename Func, typename... Args>
            auto add_task(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
            {
                using Ret = decltype(func(args...));
                auto task = std::make_shared<std::packaged_task<Ret()>>(std::bind(std::forward<Func>(func),
                                                                                  std::forward<Args>(args)...));
                auto future = task->get_future();

                size_t selected_worker_index = m_next_worker++ % m_workers.size();
                auto selected_worker = m_workers[selected_worker_index].get();

                {
                    std::unique_lock<std::mutex> lock(m_queue_mutex);
                    selected_worker->add_task_thread([task]()
                                                     { (*task)(); });
                }

                return future;
            }

            void wait(std::future<void> &future)
            {
                future.wait();
            }

            template <typename T>
            T wait_result(std::future<T> &future)
            {
                return future.get();
            }

            size_t thread_count()
            {
                return m_workers.size();
            }

        private:
            std::vector<std::unique_ptr<worker_thread>> m_workers;
            std::vector<worker_thread *> m_raw_workers;
            std::atomic<size_t> m_next_worker;
            std::mutex m_queue_mutex;
        };
    } // namespace thread_pool

} // namespace pot::experimental
