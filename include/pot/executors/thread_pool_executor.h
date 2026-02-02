#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "pot/executors/executor.h"

namespace pot
{

class thread_pool_executor : public executor
{
  public:
    thread_pool_executor(std::string name, size_t num_threads = std::thread::hardware_concurrency())
        : executor(std::move(name))
    {
        m_threads.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i)
        {
            m_threads.emplace_back([this] { worker_loop(); });
        }
    }

    ~thread_pool_executor() override { shutdown(); }

    void derived_execute(std::function<void()> &&func) override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stop)
            {
                throw std::runtime_error("Executor " + m_name + " is stopped.");
            }
            m_tasks.emplace(std::move(func));
        }
        m_cv.notify_one();
    }

    void shutdown() override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stop)
                return;
            m_stop = true;
        }

        m_cv.notify_all();

        for (auto &thread : m_threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
    }

    [[nodiscard]] size_t thread_count() const override { return m_threads.size(); }

  private:
    void worker_loop()
    {
        while (true)
        {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });

                if (m_stop && m_tasks.empty())
                {
                    return;
                }

                task = std::move(m_tasks.front());
                m_tasks.pop();
            }

            task();
        }
    }

    std::vector<std::thread> m_threads;
    std::queue<std::function<void()>> m_tasks;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop{false};
};

} // namespace pot
