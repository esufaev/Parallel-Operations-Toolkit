#include <coroutine>
#include <optional>
#include <atomic>

template <typename T>
class Task
{
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type h) : handle(h) {}

    ~Task()
    {
        if (handle)
            handle.destroy();
    }

    T get()
    {
        return handle.promise().value.value();
    }

    bool next()
    {
        if (!handle || handle.done())
            return false;
        handle.resume();
        return !handle.done();
    }

    struct promise_type
    {
        std::optional<T> value;

        auto get_return_object()
        {
            return Task{handle_type::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T v)
        {
            value = v;
            return {};
        }

        void return_value(T v)
        {
            value = v;
        }

        void unhandled_exception()
        {
            std::terminate();
        }
    };

private:
    handle_type handle;
};