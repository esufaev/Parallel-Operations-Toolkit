#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "pot/algorithms/lfqueue.h"
#include "pot/executors/executor.h"
#include "pot/utils/this_thread.h"

namespace pot
{
class thread
{
  public:
    thread(std::string name = "Thread", int64_t local_id = 0, executor *owner = nullptr)
    {
        m_thread = std::jthread([this, name = std::move(name), local_id, owner](std::stop_token st)
                                { worker_loop(st, std::move(name), local_id, owner); });
    }

    void set_name(std::string name)
    {
        run([name = std::move(name)] { pot::this_thread::set_name(name); });
    }

    template <typename FuncType, typename... Args> void run(FuncType &&func, Args &&...args)
    {
        {
            std::lock_guard lock(m_mutex);
            m_queue.emplace(
                [f = std::decay_t<FuncType>(std::forward<FuncType>(func)),
                 ... captured_args = std::decay_t<Args>(std::forward<Args>(args))]() mutable
                { std::invoke(f, std::move(captured_args)...); });
        }
        m_cv.notify_one();
    }

    void request_stop()
    {
        m_thread.request_stop();
        m_cv.notify_all();
    }

    void join()
    {
        request_stop();
        if (m_thread.joinable())
            m_thread.join();
    }

    ~thread() { join(); }

  private:
    void worker_loop(std::stop_token st, std::string name, int64_t local_id, executor *owner)
    {
        pot::this_thread::init_thread_variables(local_id, owner);
        pot::this_thread::set_name(name);

        while (!st.stop_requested())
        {
            std::function<void()> task;

            {
                std::unique_lock lock(m_mutex);
                m_cv.wait(lock, [&] { return st.stop_requested() || !m_queue.empty(); });

                if (st.stop_requested() && m_queue.empty())
                {
                    return;
                }

                if (!m_queue.empty())
                {
                    task = std::move(m_queue.front());
                    m_queue.pop();
                }
            }

            if (task)
            {
                task();
            }
        }
    }

    std::jthread m_thread;
    std::queue<std::function<void()>> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

class thread_lf
{
  public:
    thread_lf(std::string name = "Thread", int64_t local_id = 0, executor *owner = nullptr,
              size_t queue_size = 65536)
        : m_queue()
    {
        m_thread = std::jthread([this, name = std::move(name), local_id, owner](std::stop_token st)
                                { worker_loop(st, std::move(name), local_id, owner); });
    }

    void set_name(std::string name)
    {
        run([name = std::move(name)] { pot::this_thread::set_name(name); });
    }

    template <typename FuncType, typename... Args> void run(FuncType &&func, Args &&...args)
    {
        std::function<void()> task =
            [f = std::decay_t<FuncType>(std::forward<FuncType>(func)),
             ... captured_args = std::decay_t<Args>(std::forward<Args>(args))]() mutable
        { std::invoke(f, std::move(captured_args)...); };

        while (!m_queue.push(std::move(task)))
        {
            std::this_thread::yield();
        }

        m_has_work.store(true, std::memory_order_release);
        m_has_work.notify_one();
    }

    void request_stop()
    {
        m_thread.request_stop();
        m_has_work.store(true, std::memory_order_release);
        m_has_work.notify_all();
    }

    void join()
    {
        request_stop();
        if (m_thread.joinable())
            m_thread.join();
    }

    ~thread_lf() { join(); }

  private:
    void worker_loop(std::stop_token st, std::string name, int64_t local_id, executor *owner)
    {
        pot::this_thread::init_thread_variables(local_id, owner);
        pot::this_thread::set_name(name);

        while (true)
        {
            auto task_opt = m_queue.pop();

            if (task_opt.has_value())
            {
                (*task_opt)();
                continue;
            }

            if (st.stop_requested())
            {
                return;
            }

            m_has_work.store(false, std::memory_order_release);

            auto check_again = m_queue.pop();
            if (check_again.has_value())
            {
                m_has_work.store(true, std::memory_order_relaxed);
                (*check_again)();
                continue;
            }

            m_has_work.wait(false, std::memory_order_acquire);
        }
    }

    std::jthread m_thread;
    pot::algorithms::lfqueue<std::function<void()>> m_queue;
    std::atomic<bool> m_has_work{false};
};

} // namespace pot
