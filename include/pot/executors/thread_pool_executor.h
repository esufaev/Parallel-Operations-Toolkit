#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "pot/executors/executor.h"
#include "pot/threads/thread.h"

namespace pot::executors
{

class thread_pool_executor_gq : public executor
{
  public:
    thread_pool_executor_gq(std::string name,
                            size_t num_threads = std::thread::hardware_concurrency())
        : executor(std::move(name))
    {
        m_threads.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i)
        {
            std::string worker_name = m_name + "-W" + std::to_string(i);
            auto t = std::make_unique<pot::thread>(std::move(worker_name), i, this);
            t->run([this] { worker_loop(); });
            m_threads.push_back(std::move(t));
        }
    }

    ~thread_pool_executor_gq() override { shutdown(); }

    void derived_execute(std::function<void()> &&func) override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stop)
                throw std::runtime_error("Executor " + m_name + " is stopped.");
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
        m_threads.clear();
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
                    return;

                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
            if (task)
                task();
        }
    }

    std::vector<std::unique_ptr<pot::thread>> m_threads;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop{false};
};

class thread_pool_executor_lq : public executor
{
  public:
    thread_pool_executor_lq(std::string name, size_t num_threads = std::max<size_t>(
                                                  1, std::thread::hardware_concurrency()))
        : executor(std::move(name))
    {
        m_threads.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i)
        {
            std::string worker_name = m_name + "-W" + std::to_string(i);
            m_threads.push_back(std::make_unique<pot::thread>(std::move(worker_name), i, this));
        }
    }

    ~thread_pool_executor_lq() override { shutdown(); }

    void derived_execute(std::function<void()> &&func) override
    {
        if (m_stop.load(std::memory_order_acquire))
            throw std::runtime_error("Executor " + m_name + " is stopped.");

        size_t idx = m_next_thread_idx.fetch_add(1, std::memory_order_relaxed) % m_threads.size();
        m_threads[idx]->run(std::move(func));
    }

    void shutdown() override
    {
        bool expected = false;
        if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return;

        for (auto &thread : m_threads)
            thread->request_stop();
        for (auto &thread : m_threads)
            thread->join();
        m_threads.clear();
    }

    [[nodiscard]] size_t thread_count() const override { return m_threads.size(); }

  private:
    std::vector<std::unique_ptr<pot::thread>> m_threads;
    std::atomic<size_t> m_next_thread_idx{0};
    std::atomic<bool> m_stop{false};
};

class thread_pool_executor_lfgq : public executor
{
  public:
    thread_pool_executor_lfgq(std::string name,
                              size_t num_threads = std::thread::hardware_concurrency())
        : executor(std::move(name))
    {
        m_threads.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i)
        {
            std::string worker_name = m_name + "-W" + std::to_string(i);
            auto t = std::make_unique<pot::thread_lf>(std::move(worker_name), i, this);
            t->run([this] { worker_loop(); });
            m_threads.push_back(std::move(t));
        }
    }

    ~thread_pool_executor_lfgq() override { shutdown(); }

    void derived_execute(std::function<void()> &&func) override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stop)
                throw std::runtime_error("Executor " + m_name + " is stopped.");
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
        m_threads.clear();
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
                    return;

                if (!m_tasks.empty())
                {
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
            }
            if (task)
                task();
        }
    }

    std::vector<std::unique_ptr<pot::thread_lf>> m_threads;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop{false};
};

class thread_pool_executor_lflq : public executor
{
  public:
    thread_pool_executor_lflq(std::string name, size_t num_threads = std::max<size_t>(
                                                    1, std::thread::hardware_concurrency()))
        : executor(std::move(name))
    {
        m_threads.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i)
        {
            std::string worker_name = m_name + "-W" + std::to_string(i);
            m_threads.push_back(std::make_unique<pot::thread_lf>(std::move(worker_name), i, this));
        }
    }

    ~thread_pool_executor_lflq() override { shutdown(); }

    void derived_execute(std::function<void()> &&func) override
    {
        if (m_stop.load(std::memory_order_acquire))
            throw std::runtime_error("Executor " + m_name + " is stopped.");

        size_t idx = m_next_thread_idx.fetch_add(1, std::memory_order_relaxed) % m_threads.size();
        m_threads[idx]->run(std::move(func));
    }

    void shutdown() override
    {
        bool expected = false;
        if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return;

        for (auto &thread : m_threads)
            thread->request_stop();
        for (auto &thread : m_threads)
            thread->join();
        m_threads.clear();
    }

    [[nodiscard]] size_t thread_count() const override { return m_threads.size(); }

  private:
    std::vector<std::unique_ptr<pot::thread_lf>> m_threads;
    std::atomic<size_t> m_next_thread_idx{0};
    std::atomic<bool> m_stop{false};
};

} // namespace pot::executors
