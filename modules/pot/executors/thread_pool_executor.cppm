module;

#include <thread>
#include <vector>
#include <atomic>
#include <stdexcept>
#include <functional>

export module pot.executors.thread_pool_executor;

import pot.algorithms.lfqueue;
import pot.executors.executor;

export namespace pot::executors
{
    class thread_pool_executor_lfgq : public executor
    {
    public:
        thread_pool_executor_lfgq(const std::string &name,
                                  size_t thread_count = std::thread::hardware_concurrency(),
                                  size_t queue_size = 1024)
            : executor(std::move(name)), queue(queue_size), shutdown_flag(false)
        {
            if ((queue_size & (queue_size - 1)) != 0)
            {
                throw std::runtime_error("queue_size must be a power of 2");
            }
            for (size_t i = 0; i < thread_count; ++i)
            {
                workers.emplace_back([this]()
                                     { worker_loop(); });
            }
        }

        ~thread_pool_executor_lfgq()
        {
            shutdown();
        }

        void derived_execute(std::function<void()> func) override
        {
            queue.push_back_blocking(std::move(func));
        }

        void shutdown() override
        {
            shutdown_flag.store(true, std::memory_order_release);
            for (auto &worker : workers)
            {
                worker.join();
            }
        }

        size_t thread_count() const override
        {
            return workers.size();
        }

        thread_pool_executor_lfgq(const thread_pool_executor_lfgq &) = delete;
        thread_pool_executor_lfgq &operator=(const thread_pool_executor_lfgq &) = delete;
        thread_pool_executor_lfgq(thread_pool_executor_lfgq &&) = delete;
        thread_pool_executor_lfgq &operator=(thread_pool_executor_lfgq &&) = delete;

    private:
        std::vector<std::thread> workers;
        algorithms::lfqueue<std::function<void()>> queue;
        std::atomic<bool> shutdown_flag;

        void worker_loop()
        {
            while (true)
            {
                if (shutdown_flag.load(std::memory_order_acquire) && queue.is_empty())
                {
                    break;
                }
                std::function<void()> task;
                if (queue.pop(task))
                {
                    task();
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        }
    };

} // namespace pot