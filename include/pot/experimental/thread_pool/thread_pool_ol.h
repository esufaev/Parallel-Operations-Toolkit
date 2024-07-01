#pragma once
#include <any>
#include <future>
#include <queue>


namespace pot::experimental
{
    namespace details
    {
        class Task
        {
        public:
            template <typename Func, typename... Args, typename... FuncTypes>
            Task(Func(*func)(FuncTypes...), Args &&...args)
                : m_is_void{ std::is_void_v<Func> }
            {
                if constexpr (std::is_void_v<Func>)
                {
                    m_void_func = std::bind(func, std::forward<Args>(args)...);
                    m_any_func = []() -> std::any { return {}; };
                }
                else
                {
                    m_void_func = []() -> void {};
                    m_any_func = std::bind(func, std::forward<Args>(args)...);
                }
            }

            void complete(std::any result)
            {
                m_promise.set_value(result);
            }

            void complete()
            {
                m_promise.set_value(std::any());
            }

            std::future<std::any> get_future()
            {
                return m_promise.get_future();
            }

            void operator()()
            {
                m_void_func();
                m_any_func_result = m_any_func();
            }

            bool has_result() const
            {
                return !m_is_void;
            }

            std::any get_result() const
            {
                assert(!m_is_void);
                assert(m_any_func_result.has_value());
                return m_any_func_result;
            }

        private:
            std::function<void()> m_void_func;
            std::function<std::any()> m_any_func;
            std::any m_any_func_result;
            bool m_is_void;

            std::promise<std::any> m_promise;
        };
    } // namespace details

    class thread_pool_ol
    {
        using Task = details::Task;
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
            for (auto& thread : m_threads)
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }
        }

        template <typename Func, typename... Args, typename... FuncTypes>
        Task& add_task(Func(*func)(FuncTypes...), Args &&...args)
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            auto task = std::make_shared<Task>(func, std::forward<Args>(args)...);
            m_queue.push(task);
            m_queue_cv.notify_one();

            return *task;
        }

        void wait(Task& task)
        {
            auto future = task.get_future();
            future.wait();
        }

        std::any wait_result(Task& task)
        {
            auto future = task.get_future();
            return future.get();
        }

    private:
        void run()
        {
            while (true)
            {
                std::shared_ptr<Task> task;

                {
                    std::unique_lock<std::mutex> lock(m_queue_mutex);
                    m_queue_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });

                    if (m_stop && m_queue.empty()) return;

                    task = m_queue.front();
                    m_queue.pop();
                }

                (*task)();
                if (task->has_result())
                {
                    task->complete(task->get_result());
                }
                else
                {
                    task->complete();
                }
            }
        }

        std::vector<std::thread> m_threads;
        std::queue<std::shared_ptr<Task>> m_queue;
        std::mutex m_queue_mutex;
        std::condition_variable m_queue_cv;
        std::atomic<bool> m_stop;
    };
} // namespace pot::experimental