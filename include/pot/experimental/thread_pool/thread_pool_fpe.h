#pragma once
#include <cassert>
#include <functional>
#include <future>
#include <queue>
#include <ranges>
#include <thread>
#include <type_traits>
#include <utility>
#include <string>

#include "pot/traits/guards.h"

namespace pot::experimental::thread_pool
{
    namespace details_fpe
    {
        template<typename TaskType, typename TaskFunctionType, typename Func, typename...Args>
        void run_detached(std::mutex& mutex, std::condition_variable& condition, std::queue<TaskType>& tasks, Func&& func, Args&&...args)
            requires std::is_invocable_v<Func, Args...>
        {

            std::scoped_lock _(mutex);
            if constexpr (std::is_same_v<Func, TaskFunctionType>)
            {
                static_assert(sizeof...(Args) == 0, "Task function cannot have arguments");
                tasks.emplace(std::forward<Func>(func));
            }
            else
            {
                tasks.emplace(
                    [func, args...]() mutable {
                        std::invoke(func, args...);
                    });
            }

            condition.notify_one();
        }

        template<typename TaskFunctionType, typename Func, typename...Args>
        auto futured_func(Func&& func, Args&&...args)
            requires std::is_invocable_v<Func, Args...>
        {
            using return_type = std::invoke_result_t<Func, Args...>;

            auto lpromise = std::make_shared<std::promise<return_type>>();
            std::future<return_type> future = lpromise->get_future();
            auto lam = [promise = std::move(lpromise), func = std::forward<Func>(func), args...]() mutable {
                    try
                    {
                        if constexpr (std::is_void_v<return_type>)
                        {
                            std::invoke(func, args...);
                            promise->set_value();
                        }
                        else
                        {
                            return_type res = std::invoke(func, args...);
                            promise->set_value(res);
                        }
                    }
                    catch (...)
                    {
                        promise->set_exception(std::current_exception());
                    }
                };
            return std::make_pair<std::future<return_type>, TaskFunctionType>(std::move(future), (lam));
        }

        template<typename TaskType, typename TaskFunctionType, typename Func, typename...Args>
        auto run(std::mutex& mutex, std::condition_variable& condition, std::queue<TaskType>& tasks, Func&& func, Args&&...args) -> std::future<decltype(func(args...))>
            requires std::is_invocable_v<Func, Args...>
        {
            auto pair = futured_func<TaskFunctionType, Func, Args...>(std::forward<Func>(func), std::forward<Args>(args)...);

            details_fpe::run_detached<TaskType, TaskFunctionType>(mutex, condition, tasks, pair.second);

            return std::move(pair.first);
        }
    } // namespace details

    template<bool global_queue_mode>
    class thread_pool_fpe
    {
        struct empty_type{};
        class thread_fpe;
        struct Task
        {
            typedef std::function<void()> function_type;
            function_type func;
            // std::function<void()> on_finish;
        };
        class TaskGroup;

        template<typename TrueType, typename FalseType>
        using mode_type = std::conditional_t<global_queue_mode, TrueType, FalseType>;
    public:
        explicit thread_pool_fpe(size_t thread_count = std::thread::hardware_concurrency())
        {
            assert(thread_count);
            m_threads.resize(thread_count);
            for (auto idx = 0ull; idx < thread_count; ++idx)
            {
                auto& thread = m_threads[idx];

                if constexpr (global_queue_mode)
                {
                    thread = std::make_unique<thread_fpe>(
                        m_condition_mutex, m_condition, m_tasks,
                        idx, "thread " + std::to_string(idx)
                    );
                }
                else
                {
                    thread = std::make_unique<thread_fpe>(
                        idx, "thread " + std::to_string(idx)
                    );
                }
            }
        }
        ~thread_pool_fpe()
        {
            this->stop();
        }

        thread_pool_fpe(const thread_pool_fpe&) = delete;
        thread_pool_fpe(thread_pool_fpe&&) = delete;
        thread_pool_fpe& operator=(const thread_pool_fpe&) = delete;
        thread_pool_fpe& operator=(thread_pool_fpe&&) = delete;

        static thread_pool_fpe& global_instance()
        {
            static thread_pool_fpe instance;
            return instance;
        }

        [[nodiscard]] size_t thread_count() const { return m_threads.size(); }

        void stop()
        {
            for (auto&& thread : m_threads)
                thread->stop();
        }

        template<typename Func, typename...Args>
        void run_detached(Func&& func, Args&&...args)
            requires std::is_invocable_v<Func, Args...>
        {
            if constexpr (global_queue_mode)
            {
                details_fpe::run_detached<Task, typename Task::function_type, Func, Args...>(
                   m_condition_mutex, m_condition, m_tasks,
                   std::forward<Func>(func), std::forward<Args>(args)...);
            }
            else
            {
                const size_t current_thread = m_current_thread.exchange((m_current_thread + 1) % m_threads.size());

                assert(current_thread < m_threads.size());

                m_threads[current_thread]->run_detached(
                    std::forward<Func>(func), std::forward<Args>(args)...);
            }
        }


        template<typename Func, typename...Args>
        auto run(Func&& func, Args&&...args) -> std::future<decltype(func(args...))>
            requires std::is_invocable_v<Func, Args...>
        {
            if constexpr(global_queue_mode)
            {
                return details_fpe::run<Task, typename Task::function_type, Func, Args...>(
                   m_condition_mutex, m_condition, m_tasks,
                   std::forward<Func>(func), std::forward<Args>(args)...);
            }
            else
            {
                const size_t current_thread = m_current_thread.exchange((m_current_thread + 1) % m_threads.size());

                assert(current_thread < m_threads.size());

                return m_threads[current_thread]->run(
                    std::forward<Func>(func), std::forward<Args>(args)...);
            }
        }




    private:
        std::vector<std::unique_ptr<thread_fpe>> m_threads;

        mode_type<std::mutex             , empty_type> m_condition_mutex;
        mode_type<std::condition_variable, empty_type> m_condition;
        mode_type<std::queue<Task>       , empty_type> m_tasks;

        mode_type<empty_type, std::atomic_uint64_t> m_current_thread;
    };

    template<bool global_queue_mode>
    class thread_pool_fpe<global_queue_mode>::thread_fpe
    {
    public:
        explicit thread_fpe(const size_t id = 0, std::string thread_name = {})
            requires(!global_queue_mode)
            : m_id(id), m_thread_name(std::move(thread_name))
        {
            m_thread = std::thread(&thread_fpe::thread_loop, this);
        }

        explicit thread_fpe(
            std::mutex& m, std::condition_variable& cv,
            std::queue<Task>& tasks,
            const size_t id = 0, std::string thread_name = {})
            requires(global_queue_mode)
            : m_mutex(m), m_condition(cv), m_tasks(tasks),
              m_id(id), m_thread_name(std::move(thread_name))
        {
            m_thread = std::thread(&thread_fpe::thread_loop, this);
        }

        thread_fpe(const thread_fpe&) = delete;
        thread_fpe& operator=(const thread_fpe&) = delete;
        thread_fpe(thread_fpe&& other) noexcept
            :
            m_mutex(other.m_mutex),
            m_condition(other.m_condition),
            m_tasks(other.m_tasks),
            m_id(other.m_id),
            m_thread_name(std::move(other.m_thread_name)),
            m_thread(std::move(other.m_thread)) {}
        thread_fpe& operator=(thread_fpe&& other) noexcept = default;

        ~thread_fpe() { this->stop(); }

        [[nodiscard]] size_t id() const { return m_id; }
        [[nodiscard]] std::string_view thread_name() const { return m_thread_name; }
        [[nodiscard]] bool job_in_progress() const { return m_job_in_progress; }

        template<typename Func, typename...Args>
        void run_detached(Func&& func, Args&&...args)
            requires (!global_queue_mode && std::is_invocable_v<Func, Args...>)
        {
            details_fpe::run_detached<Task, typename Task::function_type, Func, Args...>(
                m_mutex, m_condition, m_tasks,
                std::forward<Func>(func), std::forward<Args>(args)...);
        }

        template<typename Func, typename...Args>
        auto run(Func&& func, Args&&...args) -> std::future<decltype(func(args...))>
            requires (!global_queue_mode && std::is_invocable_v<Func, Args...>)
        {
            return details_fpe::run<Task, typename Task::function_type, Func, Args...>(
                m_mutex, m_condition, m_tasks,
                std::forward<Func>(func), std::forward<Args>(args)...);
        }


        void thread_loop()
        {
            while (true)
            {
                //traits::guards::guard_bool<false> gb(m_job_in_progress);
                [[maybe_unused]] auto gb = traits::guards::make_guard_bool<false>(m_job_in_progress);
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
        void stop()
        {
            {
                std::unique_lock _(m_mutex);
                m_stopped = true;
            }
            m_condition.notify_all();
            if (m_thread.joinable())
                m_thread.join();
        }

    protected:
        void execute_task(const Task& task)
        {
            task.func();
        }
    private:
        // In the case of a global queue, we use an external one
        mode_type<std::mutex             &, std::mutex             > m_mutex;
        mode_type<std::condition_variable&, std::condition_variable> m_condition;
        mode_type<std::queue<Task>       &, std::queue<Task>       > m_tasks;

        size_t m_id;
        std::string m_thread_name;

        std::thread m_thread;
        std::atomic_bool m_stopped{false};
        std::atomic_bool m_job_in_progress{false};

    };

    // thread pool with global queue
    using thread_pool_gq_fpe = thread_pool_fpe<true>;

    // thread pool with local queue
    using thread_pool_lq_fpe = thread_pool_fpe<false>;


    /*template<bool global_queue_mode>
    class thread_pool_fpe<global_queue_mode>::TaskGroup
    {
        using Task = typename thread_pool_fpe<global_queue_mode>::Task;

        std::vector<>
        std::vector<std::future<void>> m_futures;

    public:
        template<typename Func, typename...Args>
        void run(Func&& func, Args&&...args)
            requires std::is_invocable_v<Func, Args...>
        {
            m_futures.emplace_back(
                thread_pool_fpe<global_queue_mode>::global_instance().run(
                    std::forward<Func>(func), std::forward<Args>(args)...));
        }

        void wait()
        {
            for (auto&& future : m_futures)
                future.wait();
        }
    };*/

} // namespace
