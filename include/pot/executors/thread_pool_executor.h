#pragma once

#include <type_traits>
#include <utility>
#include <atomic>
#include <memory>

#include "../global_thread.h"
#include "../local_thread.h"
#include "../executor.h"

namespace pot::executors
{
    template <bool global_queue_mode>
    class thread_pool_executor final : public executor
    {
    public:
        struct empty_type {};

        template <typename TrueType, typename FalseType>
        using mode_type = std::conditional_t<global_queue_mode, TrueType, FalseType>;

        using thread_type = mode_type<pot::global_thread, pot::local_thread>;

        explicit thread_pool_executor(std::string name, size_t num_threads = std::thread::hardware_concurrency())
            : executor(std::move(name)), m_shutdown(false)
        {
            m_threads.reserve(num_threads);
            if constexpr (global_queue_mode)
            {
                for (size_t i = 0; i < num_threads; ++i)
                {
                    m_threads.push_back(std::make_unique<thread_type>(
                        m_tasks_mutex, m_condition, m_tasks, i, "Thread " + std::to_string(i)));
                }
            }
            else
            {
                m_current_thread = 0;
                for (size_t i = 0; i < num_threads; ++i)
                {
                    m_threads.push_back(std::make_unique<thread_type>(i, "Thread " + std::to_string(i)));
                }
                for (auto &thread : m_threads)
                {
                    dynamic_cast<local_thread *>(thread.get())->set_other_workers(&m_threads);
                }
            }
        }

        ~thread_pool_executor() override { shutdown(); }

        void shutdown() override
        {
            m_shutdown = true;

            for (auto &thread : m_threads)
            {
                thread->request_stop();
            }

            if constexpr (global_queue_mode)
            {
                m_condition.notify_all();
            }
            else
            {
                for (auto &thread : m_threads)
                {
                    thread->notify();
                }
            }

            for (auto &thread : m_threads)
            {
                if (thread->joinable())
                {
                    thread->join();
                }
            }
        }

        [[nodiscard]] size_t thread_count() const { return m_threads.size(); }

    protected:
        void derived_execute(std::function<void()> func) override
        {
            if constexpr (global_queue_mode)
            {
                {
                    std::lock_guard lock(m_tasks_mutex);
                    m_tasks.emplace(std::move(func));
                }
                m_condition.notify_one();
            }
            else
            {
                m_threads[m_current_thread++ % m_threads.size()]->run_detached(std::move(func));
            }
        }

    private:
        std::atomic_bool m_shutdown;
        std::vector<std::unique_ptr<thread_type>> m_threads;

        mode_type<std::mutex, empty_type> m_tasks_mutex;
        mode_type<std::condition_variable, empty_type> m_condition;
        mode_type<std::queue<std::function<void()>>, empty_type> m_tasks;

        mode_type<empty_type, std::atomic_uint64_t> m_current_thread;
    };

    using thread_pool_executor_gq = thread_pool_executor<true>;
    using thread_pool_executor_lq = thread_pool_executor<false>;
}
