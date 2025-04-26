#include <list>
#include <coroutine>

namespace pot::coroutines
{
    using coro_t = std::coroutine_handle<>;
    
    class async_condition_variable
    {
        struct awaiter;

        std::list<awaiter> m_awaiter_list;
        bool m_set_state;

        struct awaiter
        {
            async_condition_variable &awaiter_event;
            coro_t coroutine_handle = nullptr;
            awaiter(async_condition_variable &event) noexcept : awaiter_event(event) {}

            bool await_ready() const noexcept { return awaiter_event.is_set(); }

            void await_suspend(coro_t c) noexcept
            {
                coroutine_handle = c;
                awaiter_event.push_awaiter(*this);
            }

            void await_resume() noexcept { awaiter_event.reset(); }
        };

    public:
        async_condition_variable(bool set = false) : m_set_state{set} {}

        async_condition_variable(const async_condition_variable &) = delete;
        async_condition_variable &operator=(const async_condition_variable &) = delete;
        async_condition_variable(async_condition_variable &&) = delete;
        async_condition_variable &operator=(async_condition_variable &&) = delete;

        bool is_set() const noexcept { return m_set_state; }

        void push_awaiter(awaiter a) { m_awaiter_list.push_back(a); }

        awaiter operator co_await() noexcept { return awaiter{*this}; }

        void set() noexcept
        {
            m_set_state = true;
            std::list<awaiter> resume_list;
            resume_list.splice(resume_list.begin(), m_awaiter_list);
            for (auto s : resume_list)
                s.coroutine_handle.resume();
        }

        void stop() noexcept
        {
            m_set_state = false;
            m_awaiter_list.clear();
        }

        void reset() noexcept { m_set_state = false; }
    };
}