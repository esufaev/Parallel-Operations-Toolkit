#include "pot/experimental/thread_pool/thread_pool_GQ_fpe.h"

#include <cassert>

pot::experimental::thread_pool::thread_pool_gq_fpe::thread_pool_gq_fpe(const size_t thread_count)
{
    assert(thread_count);
    m_threads.resize(thread_count);
    for(auto&& [idx, thread] : std::views::enumerate(m_threads))
    {
        thread = std::make_unique<thread_gq_fpe>(
            m_condition_mutex, m_condition, m_tasks,
            idx, std::format("thread {}", idx)
            );
    }
}

pot::experimental::thread_pool::thread_pool_gq_fpe::~thread_pool_gq_fpe()
{
    this->stop();
}

pot::experimental::thread_pool::thread_pool_gq_fpe& pot::experimental::thread_pool::thread_pool_gq_fpe::
global_instance()
{
    static thread_pool_gq_fpe instance;
    return instance;
}

size_t pot::experimental::thread_pool::thread_pool_gq_fpe::thread_count() const
{
    return m_threads.size();
}

void pot::experimental::thread_pool::thread_pool_gq_fpe::stop()
{
    for(auto&& thread : m_threads)
        thread->stop();
}

pot::experimental::thread_pool::thread_pool_gq_fpe::thread_gq_fpe::thread_gq_fpe(std::mutex& m,
    std::condition_variable& cv, std::queue<Task>& tasks, const size_t id, std::string thread_name)
        :
        m_mutex(m),
        m_condition(cv),
        m_tasks(tasks),
        m_id(id),
        m_thread_name(std::move(thread_name))
{
    m_thread = std::thread(&thread_gq_fpe::run, this);
}

pot::experimental::thread_pool::thread_pool_gq_fpe::thread_gq_fpe::thread_gq_fpe(thread_gq_fpe&& other) noexcept
    :
    m_mutex(other.m_mutex),
    m_condition(other.m_condition),
    m_tasks(other.m_tasks),
    m_id(other.m_id),
    m_thread_name(std::move(other.m_thread_name)),
    m_thread(std::move(other.m_thread))
{

}

pot::experimental::thread_pool::thread_pool_gq_fpe::thread_gq_fpe::~thread_gq_fpe()
{
    this->stop();
}

size_t pot::experimental::thread_pool::thread_pool_gq_fpe::thread_gq_fpe::id() const
{
    return m_id;
}

std::string_view pot::experimental::thread_pool::thread_pool_gq_fpe::thread_gq_fpe::thread_name()
{
    return m_thread_name;
}

void pot::experimental::thread_pool::thread_pool_gq_fpe::thread_gq_fpe::run()
{
    while (true)
    {
        Task task;
        {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, [this] { return m_stopped || !m_tasks.empty(); });

            if (m_stopped)
                break;

            task = m_tasks.front();
            m_tasks.pop();
        } // auto unlock

        execute_task(task);
    }
}

void pot::experimental::thread_pool::thread_pool_gq_fpe::thread_gq_fpe::stop()
{
    std::unique_lock lock(m_mutex);
    m_stopped = true;
    m_condition.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void pot::experimental::thread_pool::thread_pool_gq_fpe::thread_gq_fpe::execute_task(const Task& task)
{
    task.func();
}
