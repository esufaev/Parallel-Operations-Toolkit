#include <thread>
#include <vector>
#include <atomic>

#include "pot/algorithms/lfqueue.h"
#include "pot/executors/executor.h"

namespace pot::executors
{
    class thread_pool_executor_lfgq final : public executor
    {
    private:
        std::vector<std::thread> m_workers;
        pot::algorithms::lfqueue<std::function<void()>> m_queue;
        std::atomic<bool> m_shutdown_flag;

        void worker_loop()
        {
            while (true)
            {
                if (m_shutdown_flag.load(std::memory_order_acquire) && m_queue.is_empty()) { break; }
                std::function<void()> task;
                if (m_queue.pop(task)) { task(); }
                else { std::this_thread::yield(); }
            }
        }

    public:
        thread_pool_executor_lfgq(const std::string &name,
                                  size_t thread_count = std::thread::hardware_concurrency(),
                                  size_t queue_size = 1024)
            : executor(std::move(name)), m_queue(queue_size), m_shutdown_flag(false)
        {
            if ((queue_size & (queue_size - 1)) != 0)
            {
                throw std::runtime_error("queue_size must be a power of 2");
            }
            for (size_t i = 0; i < thread_count; ++i)
            {
                m_workers.emplace_back([this]()
                                     { worker_loop(); });
            }
        }

        ~thread_pool_executor_lfgq() { shutdown(); }

        void derived_execute(std::function<void()> func) override { m_queue.push_back_blocking(std::move(func)); }

        void shutdown() override
        {
            m_shutdown_flag.store(true, std::memory_order_release);
            for (auto &worker : m_workers)
            {
                worker.join();
            }
        }

        size_t thread_count() const override { return m_workers.size(); }

        thread_pool_executor_lfgq(const thread_pool_executor_lfgq &) = delete;
        thread_pool_executor_lfgq &operator=(const thread_pool_executor_lfgq &) = delete;
        thread_pool_executor_lfgq(thread_pool_executor_lfgq &&) = delete;
        thread_pool_executor_lfgq &operator=(thread_pool_executor_lfgq &&) = delete;
    };

} // namespace pot