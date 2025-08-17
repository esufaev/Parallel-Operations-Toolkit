#include <thread>
#include <vector>
#include <atomic>
#include <stdexcept>
#include <functional>

#include "pot/algorithms/lfqueue.h"
#include "pot/executors/executor.h"

namespace pot::executors
{
    /**
     * @brief Thread pool executor using a global lock-free queue.
     *
     * Schedules tasks into a bounded lock-free queue (`lfqueue`) and executes them
     * on a fixed number of worker threads. Tasks are submitted via `run`, `lazy_run`
     * or `run_detached` from the base `executor` interface.
     *
     * @param name Human-readable executor name (in ctor).
     * @param thread_count Number of worker threads to start. Defaults to `std::thread::hardware_concurrency()`.
     * @param queue_size Size of the lock-free queue. Must be a power of two. Default = 1024.
     *
     * @return derived_execute(func) schedules a function into the queue\n  
     * @return shutdown() signals stop, drains queue, and joins all workers\n  
     * @return thread_count() returns number of worker threads.  
     */
    class thread_pool_executor_lfgq : public executor
    {
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
    };

} // namespace pot