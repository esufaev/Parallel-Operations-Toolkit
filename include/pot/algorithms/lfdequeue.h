#pragma once

// temp

#include <atomic>
#include <optional>

namespace pot::algorithms
{
  template <typename T>
class lfdequeue
{
public:
    struct stealer_token
    {
        std::atomic<long> id_last_used;
        std::atomic<bool> was_idle;
        stealer_token *next;
    };

    lfdequeue();
    ~lfdequeue();

    void push(const T &item);
    [[nodiscard]] std::optional<T> pop();

    [[nodiscard]] stealer_token *register_stealer();
    [[nodiscard]] std::optional<T> pop_back(stealer_token *token);

private:
    struct ring_buffer
    {
        long id;
        int log_size;
        T *segment;
        ring_buffer *next;

        ring_buffer(int ls, long i);
        ~ring_buffer();

        long size() const;
        T get(long i) const;
        void put(long i, T item);
        ring_buffer *resize(long b, long t, int delta);
    };

    static constexpr int log_initial_size = 4;

    alignas(64) std::atomic<long> top{0};
    alignas(64) std::atomic<long> bottom{0};

    ring_buffer *unlinked{nullptr};
    std::atomic<stealer_token *> id_list{nullptr};
    std::atomic<ring_buffer *> active_buffer;

    void reclaim_buffers(ring_buffer *new_buffer);
};
}

template <typename T>
pot::algorithms::lfdequeue<T>::ring_buffer::ring_buffer(int ls, long i)
    : id(i), log_size(ls), next(nullptr)
{
    segment = new T[1 << log_size];
}

template <typename T>
pot::algorithms::lfdequeue<T>::ring_buffer::~ring_buffer()
{
    delete[] segment;
}

template <typename T>
long pot::algorithms::lfdequeue<T>::ring_buffer::size() const
{
    return static_cast<long>(1 << log_size);
}

template <typename T>
T pot::algorithms::lfdequeue<T>::ring_buffer::get(long i) const
{
    return segment[i % size()];
}

template <typename T>
void pot::algorithms::lfdequeue<T>::ring_buffer::put(long i, T item)
{
    segment[i % size()] = item;
}

template <typename T>
typename pot::algorithms::lfdequeue<T>::ring_buffer *pot::algorithms::lfdequeue<T>::ring_buffer::resize(long b, long t, int delta)
{
    auto buffer = new ring_buffer(log_size + delta, id + 1);
    for (auto i = t; i < b; ++i)
    {
        buffer->put(i, get(i));
    }
    next = buffer;
    return buffer;
}

template <typename T>
pot::algorithms::lfdequeue<T>::lfdequeue()
    : active_buffer(new ring_buffer(log_initial_size, 0))
{
}

template <typename T>
pot::algorithms::lfdequeue<T>::~lfdequeue()
{
    auto b = active_buffer.load(std::memory_order_relaxed);

    while (unlinked && unlinked != b)
    {
        auto reclaimed = unlinked;
        unlinked = unlinked->next;
        delete reclaimed;
    }

    delete b;

    auto head = id_list.load(std::memory_order_relaxed);
    while (head)
    {
        auto reclaimed = head;
        head = head->next;
        delete reclaimed;
    }
}

template <typename T>
void pot::algorithms::lfdequeue<T>::push(const T &item)
{
    auto b = bottom.load(std::memory_order_relaxed);
    auto t = top.load(std::memory_order_acquire);
    auto a = active_buffer.load(std::memory_order_relaxed);

    auto current_size = b - t;
    if (current_size >= a->size() - 1)
    {
        unlinked = unlinked ? unlinked : a;
        a = a->resize(b, t, 1);
        active_buffer.store(a, std::memory_order_release);
    }

    if (unlinked)
    {
        reclaim_buffers(a);
    }

    a->put(b, item);
    std::atomic_thread_fence(std::memory_order_release);
    bottom.store(b + 1, std::memory_order_relaxed);
}

template <typename T>
std::optional<T> pot::algorithms::lfdequeue<T>::pop()
{
    auto b = bottom.load(std::memory_order_relaxed);
    auto a = active_buffer.load(std::memory_order_acquire);

    bottom.store(b - 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto t = top.load(std::memory_order_relaxed);

    auto current_size = b - t;
    std::optional<T> popped = std::nullopt;

    if (current_size <= 0)
    {
        bottom.store(b, std::memory_order_relaxed);
    }
    else if (current_size == 1)
    {
        if (top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
        {
            popped = a->get(t);
        }
        bottom.store(b, std::memory_order_relaxed);
    }
    else
    {
        popped = a->get(b - 1);

        if (current_size <= a->size() / 3 && current_size > (1 << log_initial_size))
        {
            unlinked = unlinked ? unlinked : a;
            a = a->resize(b, t, -1);
            active_buffer.store(a, std::memory_order_release);
        }

        if (unlinked)
        {
            reclaim_buffers(a);
        }
    }

    return popped;
}

template <typename T>
typename pot::algorithms::lfdequeue<T>::stealer_token *pot::algorithms::lfdequeue<T>::register_stealer()
{
    auto token = new stealer_token{{0}, {true}, nullptr};
    token->next = id_list.load(std::memory_order_relaxed);

    while (!id_list.compare_exchange_weak(token->next, token))
    {
        token->next = id_list.load(std::memory_order_relaxed);
    }

    return token;
}

template <typename T>
std::optional<T> pot::algorithms::lfdequeue<T>::pop_back(stealer_token *token)
{
    token->was_idle.store(false, std::memory_order_release);

    auto t = top.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto b = bottom.load(std::memory_order_acquire);

    long current_size = b - t;
    std::optional<T> stolen = std::nullopt;

    if (current_size > 0)
    {
        auto a = active_buffer.load(std::memory_order_consume);
        if (top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
        {
            stolen = a->get(t);
        }
    }

    token->was_idle.store(true, std::memory_order_release);

    auto a = active_buffer.load(std::memory_order_consume);
    token->id_last_used.store(a->id, std::memory_order_relaxed);

    return stolen;
}

template <typename T>
void pot::algorithms::lfdequeue<T>::reclaim_buffers(ring_buffer *new_buffer)
{
    auto min_id = new_buffer->id;
    auto head = id_list.load(std::memory_order_relaxed);

    while (head)
    {
        auto idle = head->was_idle.load(std::memory_order_acquire);
        if (!idle)
        {
            auto last_used_id = head->id_last_used.load(std::memory_order_relaxed);
            min_id = std::min(min_id, last_used_id);
        }
        head = head->next;
    }

    while (unlinked && unlinked->id < min_id)
    {
        auto reclaimed = unlinked;
        unlinked = unlinked->next;
        delete reclaimed;
    }
}
