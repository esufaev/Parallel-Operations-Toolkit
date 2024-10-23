#pragma once

#include "thread.h"

namespace pot
{
    class global_thread final : public thread
    {
    public:
        explicit global_thread(std::mutex &m, std::condition_variable &cv,
                               std::queue<std::function<void()>> &tasks,
                               const size_t id = 0, std::string thread_name = {})
            : thread(id, std::move(thread_name)), m_mutex_ref(m), m_condition_ref(cv), m_tasks_ref(tasks)
        {
            m_thread = std::jthread(&global_thread::thread_loop, this, m_stop_source.get_token());
        }

        ~global_thread() override = default;

        bool joinable() const override
        {
            return m_thread.joinable();
        }

        void join() override
        {
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

        void request_stop() override
        {
            m_stop_source.request_stop();
        }

        void notify() override
        {
            m_condition_ref.notify_one();
        }

    private:
        void thread_loop(std::stop_token stop_token)
        {
            while (!stop_token.stop_requested())
            {
                std::function<void()> task;
                {
                    std::unique_lock lock(m_mutex_ref);
                    m_condition_ref.wait(lock, [&]
                                         { return stop_token.stop_requested() || !m_tasks_ref.empty(); });

                    if (stop_token.stop_requested())
                    {
                        return;
                    }

                    assert(not m_tasks_ref.empty());

                    task = std::move(m_tasks_ref.front());
                    m_tasks_ref.pop();
                }

                task();
            }
        }

        std::mutex &m_mutex_ref;
        std::condition_variable &m_condition_ref;
        std::queue<std::function<void()>> &m_tasks_ref;
    };
}