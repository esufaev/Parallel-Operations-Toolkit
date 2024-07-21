#pragma once

#include "thread.h"

namespace pot
{
    class local_thread final : public thread
    {
    public:
        explicit local_thread(const size_t id = 0, std::string thread_name = {})
            : thread(id, std::move(thread_name))
        {
            m_thread = std::jthread(&local_thread::thread_loop, this, m_stop_source.get_token());
        }

        ~local_thread() override = default;

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
            m_condition.notify_one();
        }

        void set_other_workers(std::vector<std::unique_ptr<local_thread>> *other_workers)
        {
            m_other_workers = other_workers;
        }

    private:
        void thread_loop(std::stop_token stop_token)
        {
            while (!stop_token.stop_requested())
            {
                std::function<void()> task;
                {
                    std::unique_lock lock(m_mutex);
                    m_condition.wait(lock, [&]
                                     { return stop_token.stop_requested() || !m_tasks.empty(); });

                    if (stop_token.stop_requested() && m_tasks.empty())
                    {
                        return;
                    }

                    if (!m_tasks.empty())
                    {
                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                }

                if (task)
                {
                    task();
                }

                if (m_tasks.empty() && m_other_workers)
                {
                    task = steal_task();
                    if (task)
                    {
                        task();
                    }
                }
            }
        }

        std::function<void()> steal_task()
        {
            for (auto &worker : *m_other_workers)
            {
                if (worker.get() == this)
                {
                    continue;
                }

                std::unique_lock lock(worker->m_mutex);
                if (!worker->m_tasks.empty())
                {
                    auto task = std::move(worker->m_tasks.front());
                    worker->m_tasks.pop();
                    return task;
                }
            }
            return nullptr;
        }

        std::vector<std::unique_ptr<local_thread>> *m_other_workers{nullptr};
    };
}