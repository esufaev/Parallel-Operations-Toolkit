#pragma once
#include "pot/algorithms/lfqueue.h"
#include "pot/coroutines/task.h"
#include "pot/utils/this_thread.h"
#include "pot/utils/unique_function.h"
#include <stop_token>
#include <thread>
#include <utility>

namespace pot
{
class thread
{
  public:
    thread(size_t queue_size = 1024, std::string name = "Thread", int64_t local_id = 0)
        : m_thread_queue(queue_size)
    {
        m_thread = std::jthread([this](std::stop_token st) { worker_loop(st); });
    }

    void set_name(std::string name)
    {
        run([name] { pot::this_thread::set_name(name); });
    }

    template <typename FuncType, typename... Args> bool run(FuncType &&func, Args &&...args)
    {
        bool result =
            m_thread_queue.push_back([f = std::move(func), ... args = std::move(args)]() mutable
                                     { f(std::forward<Args>(args)...); });
        return result;
    }

    void request_stop() { m_thread.request_stop(); }

    void join()
    {
        while (!m_thread_queue.is_empty())
            pot::this_thread::yield();
        request_stop();
        if (m_thread.joinable())
            m_thread.join();
    }

    ~thread() { join(); }

  private:
    void worker_loop(std::stop_token st)
    {
        while (!st.stop_requested())
        {
            pot::utils::unique_function_once task;
            if (m_thread_queue.pop(task))
            {
                if (task)
                    task();
            }
        }
    }

    std::jthread m_thread;
    pot::algorithms::lfqueue<pot::utils::unique_function_once> m_thread_queue;
};
} // namespace pot
