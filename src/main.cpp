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

namespace tools
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
            auto task = std::make_shared<Task<Ret>>(func, std::forward<Args>(args)...);
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
}

std::mutex cout_mtx;
void print(std::string mes, int i)
{
    cout_mtx.lock();
    std::cout << mes + " " << i << std::endl;
    cout_mtx.unlock();
}

std::string ping(std::string str, int i)
{
    std::string result = str + " " + std::to_string(i);
    return result;
}

int main()
{
    tools::thread_pool_ol tp(10);
    for (int i = 0; i < 10; i++)
    {
        auto future = tp.add_task(print, "Task: ", i + 1);
        tp.wait(future);
    }

    tools::thread_pool_ol tp1(10);
    for (int i = 0; i < 10; i++)
    {
        auto future = tp1.add_task(ping, "Ping: ", i + 1);
        std::string result = tp1.wait_result<std::string>(future);
        std::cout << result << std::endl;
    }

    return 0;
}
