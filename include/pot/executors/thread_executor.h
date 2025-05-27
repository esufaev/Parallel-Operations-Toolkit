#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "pot/executors/executor.h"

namespace pot::executors
{
    class thread_executor;
}

class pot::executors::thread_executor final : public executor
{
public:
    explicit thread_executor(std::string name);

    ~thread_executor() override;

    void shutdown() override;

protected:
    void derived_execute(std::function<void()> func) override;

private:
    void thread_loop();

    bool m_shutdown;
    std::thread m_thread;

    std::queue<std::function<void()>> m_queue;
    std::mutex m_queue_mtx;
    std::condition_variable m_queue_cv;

};