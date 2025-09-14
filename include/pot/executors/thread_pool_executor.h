#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <stdexcept>

#include "pot/executors/executor.h"
#include "pot/algorithms/lfqueue.h"
#include "pot/utils/unique_function.h"

namespace pot::executors
{
    class thread_pool_executor_lflqt final : public executor
    {
    public:
        explicit thread_pool_executor_lflqt(
            std::string name,
            std::size_t thread_count = std::thread::hardware_concurrency(),
            std::size_t queue_capacity = 1024)
            : executor(std::move(name)), m_thread_count(thread_count ? thread_count : 1), m_queue_capacity(queue_capacity ? queue_capacity : 1<<12)
        {
            m_queues.reserve(m_thread_count);
            for (std::size_t i = 0; i < m_thread_count; ++i)
                m_queues.emplace_back(std::make_unique<pot::algorithms::lfqueue<pot::utils::unique_function_once>>(m_queue_capacity));

            m_workers.reserve(m_thread_count);
            for (std::size_t i = 0; i < m_thread_count; ++i)
                m_workers.emplace_back(&thread_pool_executor_lflqt::worker_loop, this, i);
        }

        ~thread_pool_executor_lflqt() override
        {
            shutdown();
        }

        std::size_t thread_count() const override { return m_thread_count; }

        void stop() noexcept
        {
            bool expected = false;
            if (!m_stopping.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return;
            m_cv.notify_all();
        }

        void join()
        {
            for (auto &t : m_workers)
            {
                if (t.joinable())
                    t.join();
            }
        }

        void shutdown() override
        {
            stop();
            join();
        }

    protected:
        void derived_execute(pot::utils::unique_function_once &&f) override
        {
            if (m_stopping.load(std::memory_order_acquire))
                throw std::runtime_error("shutting down");

            std::size_t idx = m_round_robin.fetch_add(1, std::memory_order_relaxed) % m_thread_count;
            
            if (!m_queues[idx]->push_back(std::move(f)))
                throw std::runtime_error("cant push_back lfqueue");
            
            m_pending.fetch_add(1, std::memory_order_release);
            m_cv.notify_one();
        }

    private:
        static constexpr std::size_t npos = static_cast<std::size_t>(-1); // temp
        static inline thread_local std::size_t t_worker_index = npos;

        std::size_t current_worker_index() const noexcept { return t_worker_index; }

        void worker_loop(std::size_t my_index)
        {
            t_worker_index = my_index;

            auto try_pop_execute = [&](std::size_t from_index) -> bool
            {
                pot::utils::unique_function_once task;
                if (m_queues[from_index]->pop(task))
                {
                    try 
                    {
                        task();
                    } 
                    catch (...) 
                    {
                        m_pending.fetch_sub(1, std::memory_order_acq_rel);
                        throw;
                    }
                    m_pending.fetch_sub(1, std::memory_order_acq_rel);
                    return true;
                }
                return false;
            };

            while (true)
            {
                if (try_pop_execute(my_index))
                    continue;

                bool stolen = false;
                for (std::size_t k = 0; k < m_thread_count; ++k)
                {
                    std::size_t victim = (my_index + k) % m_thread_count;
                    if (try_pop_execute(victim))
                    {
                        stolen = true;
                        break;
                    }
                }

                if (stolen)
                    continue;

                if (m_stopping.load(std::memory_order_acquire) && m_pending.load(std::memory_order_acquire) == 0)
                {
                    break;
                }

                std::unique_lock<std::mutex> lk(m_cv_mtx);
                m_cv.wait(lk, [&]
                          { return m_stopping.load(std::memory_order_acquire) || m_pending.load(std::memory_order_acquire) > 0; });
            }

            t_worker_index = npos;
        }

    private:
        const std::size_t m_thread_count;
        const std::size_t m_queue_capacity;

        std::vector<std::unique_ptr<pot::algorithms::lfqueue<pot::utils::unique_function_once>>> m_queues;
        std::vector<std::thread> m_workers;

        std::atomic<bool> m_stopping{false};
        std::atomic<size_t> m_pending{0};
        std::atomic<size_t> m_round_robin{0};

        std::mutex m_cv_mtx;
        std::condition_variable m_cv;
    };
} // namespace pot
