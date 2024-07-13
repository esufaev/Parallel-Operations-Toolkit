#include "pot/executors/thread_executor.h"

pot::executors::thread_executor::thread_executor(std::string name)
    :
    executor(std::move(name)),
    m_shutdown(false),
    m_thread(&thread_executor::thread_loop, this)
{
}

pot::executors::thread_executor::~thread_executor()
{
    shutdown();
}

void pot::executors::thread_executor::derived_execute(std::function<void()> func)
{
    {
        std::unique_lock<std::mutex> lock(m_queue_mtx);
        m_queue.push(std::move(func));
    }
    m_queue_cv.notify_one();
}


void pot::executors::thread_executor::shutdown()
{
    {
        std::unique_lock<std::mutex> lock(m_queue_mtx);
        m_shutdown = true;
    }
    m_queue_cv.notify_one();
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

void pot::executors::thread_executor::thread_loop()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_queue_mtx);
            m_queue_cv.wait(lock, [this]()
                            { return m_shutdown || !m_queue.empty(); });

            if (m_shutdown && m_queue.empty())
                return;

            task = std::move(m_queue.front());
            m_queue.pop();
        }
        task();
    }
}

