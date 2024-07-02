#pragma once
#include <thread>
#include <future>
#include <queue>

namespace pot::experimental::thread_pool
{
    template<typename T>
    class inline_promise {
        alignas(std::promise<T>) std::byte storage[sizeof(std::promise<T>)];
        std::promise<T>* ptr = nullptr;

    public:
        inline_promise() : ptr(std::construct_at(reinterpret_cast<std::promise<T>*>(storage))) {}
        ~inline_promise() { if (ptr) std::destroy_at(ptr); }

        inline_promise(inline_promise&& other) noexcept
            : ptr(std::exchange(other.ptr, nullptr)) {
            if (ptr) {
                std::construct_at(reinterpret_cast<std::promise<T>*>(storage), std::move(*other.ptr));
                std::destroy_at(other.ptr);
                ptr = reinterpret_cast<std::promise<T>*>(storage);
            }
        }

        inline_promise& operator=(inline_promise&& other) noexcept {
            if (this != &other) {
                if (ptr) std::destroy_at(ptr);
                ptr = std::exchange(other.ptr, nullptr);
                if (ptr) {
                    std::construct_at(reinterpret_cast<std::promise<T>*>(storage), std::move(*other.ptr));
                    std::destroy_at(other.ptr);
                    ptr = reinterpret_cast<std::promise<T>*>(storage);
                }
            }
            return *this;
        }

        std::promise<T>& operator*() { return *ptr; }
        const std::promise<T>& operator*() const { return *ptr; }
        std::promise<T>* operator->() { return ptr; }
        const std::promise<T>* operator->() const { return ptr; }
    };

    class thread_pool_gq_fpe
    {
        class thread_gq_fpe;
        struct Task
        {
            using function_type = std::function<void()>;
            function_type func;
            // std::function<void()> on_finish;
        };
    public:
        explicit thread_pool_gq_fpe(size_t thread_count = std::thread::hardware_concurrency());
        ~thread_pool_gq_fpe();

        static thread_pool_gq_fpe& global_instance();

        [[nodiscard]] size_t thread_count() const;

        void stop();

        template<typename Func, typename...Args>
        void run_detached(Func&& func, Args&&...args)
            requires std::is_invocable_v<Func, Args...>
        {
            using return_type = std::invoke_result_t<Func, Args...>;
            std::scoped_lock _(m_condition_mutex);
            if constexpr (std::is_same_v<return_type, Task::function_type>)
            {
                static_assert(sizeof...(Args) == 0, "Task function cannot have arguments");
                m_tasks.emplace(std::forward<Func>(func));
            }
            else
            {
                m_tasks.emplace(
                    [func, args...]() mutable {
                        std::invoke(func, args...);
                    });
            }

            m_condition.notify_one();
        }


        template<typename Func, typename...Args>
        auto run(Func&& func, Args&&...args) -> std::future<decltype(func(args...))>
            requires std::is_invocable_v<Func, Args...>
        {
            using return_type = std::invoke_result_t<Func, Args...>;


            auto lpromise = std::make_shared<std::promise<return_type>>();
            auto future = lpromise->get_future();
            this->run_detached([promise = std::move(lpromise), func = std::forward<Func>(func), args...]() mutable {
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
                catch(...)
                {
                    promise->set_exception(std::current_exception());
                }
            });
            return future;
        }




    private:
        std::vector<std::unique_ptr<thread_gq_fpe>> m_threads;

        std::queue<Task> m_tasks;
        std::condition_variable m_condition;
        std::mutex m_condition_mutex;
    };

    class thread_pool_gq_fpe::thread_gq_fpe
    {
    public:
        explicit thread_gq_fpe(
            std::mutex& m, std::condition_variable& cv,
            std::queue<Task>& tasks,
            size_t id = 0, std::string thread_name = {});
        thread_gq_fpe(const thread_gq_fpe&) = delete;
        thread_gq_fpe(thread_gq_fpe&&) noexcept;
        ~thread_gq_fpe();



        [[nodiscard]] size_t id() const;
        [[nodiscard]] std::string_view thread_name();


        void run();
        void stop();

    protected:
        void execute_task(const Task& task);
    private:
        std::mutex& m_mutex;
        std::condition_variable& m_condition;
        std::queue<Task>& m_tasks;

        size_t m_id;
        std::string m_thread_name;

        std::thread m_thread;
        std::atomic_bool m_stopped{false};

    };

}
