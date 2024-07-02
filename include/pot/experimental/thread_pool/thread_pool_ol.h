#pragma once
#include <any>
#include <future>
#include <queue>


namespace pot::experimental
{
    namespace details
    {
        template <typename Ret>
        class Task
        {
        public:
            template <typename Func, typename... Args>
            Task(Func func, Args &&...args)
            {
                m_task = std::packaged_task<Ret()>([func, args...]()
                                                { return func(args...); });
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
    } // namespace details

    class thread_pool_ol
    {
    public:
        thread_pool_ol(int pool_size) : m_stop(false)
        {
            for (int i = 0; i < pool_size; ++i)
            {
                m_threads.emplace_back(&thread_pool_ol::run, this);
            }
        }

        ~thread_pool_ol()
        {
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_stop = true;
            }
            m_queue_cv.notify_all();
            for (auto &thread : m_threads)
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }
        }

        template <typename Func, typename... Args>
        auto add_task(Func func, Args &&...args)
        {
            using Ret = decltype(func(args...));
            auto task = std::make_shared<details::Task<Ret>>(func, std::forward<Args>(args)...);
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_queue.push([task]()
                             { (*task)(); });
            }
            m_queue_cv.notify_one();

            return task->get_future();
        }

        template <typename Future>
        void wait(Future &future)
        {
            future.wait();
        }

        template <typename ResultType, typename Future>
        ResultType wait_result(Future &future)
        {
            return future.get();
        }

    private:
        void run()
        {
            while (true)
            {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(m_queue_mutex);
                    m_queue_cv.wait(lock, [this]
                                    { return m_stop || !m_queue.empty(); });

                    if (m_stop && m_queue.empty())
                        return;

                    task = std::move(m_queue.front());
                    m_queue.pop();
                }

                task();
            }
        }

        std::vector<std::thread> m_threads;

        std::queue<std::function<void()>> m_queue;
        std::mutex m_queue_mutex;
        std::condition_variable m_queue_cv;

        std::atomic<bool> m_stop;
    };
} // namespace pot::experimental