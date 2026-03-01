#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stack>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "pot/executors/executor.h"
#include "pot/utils/this_thread.h"

namespace pot
{

	struct work_queue
	{
		std::queue<std::function<void()>> tasks;
		std::mutex mtx;
	};

	struct work_stack
	{
		std::stack<std::function<void()>> tasks;
		std::mutex mtx;
	};

	struct work_deque
	{
		std::deque<std::function<void()>> tasks;
		std::mutex mtx;
	};

} // namespace pot

namespace pot::executors
{
	class thread_pool_executor_gq final : public executor
	{
	public:
		thread_pool_executor_gq(std::string name,
								size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency()))
			: executor(std::move(name))
		{
			m_threads.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				std::string worker_name = m_name + "-W" + std::to_string(i);
				m_threads.emplace_back([this, worker_name = std::move(worker_name), i](std::stop_token st)
									   { worker_loop(std::move(st), std::move(worker_name), i); });
			}
		}

		~thread_pool_executor_gq() override
		{
			shutdown();
		}

		void derived_execute(std::function<void()> &&func, pot::coroutines::details::task_meta *meta = nullptr) override
		{
			if (m_stop.load(std::memory_order_acquire))
				throw std::runtime_error("Executor " + m_name + " is stopped.");

			{
				std::lock_guard lock(m_queue.mtx);
				m_queue.tasks.emplace(std::move(func));
			}
			m_notifier.fetch_add(1, std::memory_order_release);
			m_notifier.notify_one();
		}

		void shutdown() override
		{
			bool expected = false;
			if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
				return;

			for (auto &thread : m_threads)
				thread.request_stop();

			m_notifier.fetch_add(m_threads.size(), std::memory_order_release);
			m_notifier.notify_all();

			m_threads.clear();
		}

		[[nodiscard]] size_t thread_count() const override
		{
			return m_threads.size();
		}

	private:
		void worker_loop(std::stop_token st, std::string name, int64_t local_id)
		{
			pot::this_thread::init_thread_variables(local_id, this);
			pot::this_thread::set_name(name);

			while (!st.stop_requested())
			{
				uint64_t wait_val = m_notifier.load(std::memory_order_acquire);
				std::function<void()> task;

				{
					std::lock_guard lock(m_queue.mtx);
					if (!m_queue.tasks.empty())
					{
						task = std::move(m_queue.tasks.front());
						m_queue.tasks.pop();
					}
				}

				if (task)
				{
					task();
				}
				else
				{
					if (st.stop_requested())
						return;
					m_notifier.wait(wait_val, std::memory_order_acquire);
				}
			}
		}

		std::vector<std::jthread> m_threads;
		pot::work_queue m_queue;
		std::atomic<uint64_t> m_notifier{0};
		std::atomic<bool> m_stop{false};
	};

	class thread_pool_executor_lq : public executor
	{
		struct thread_context
		{
			pot::work_queue queue;
			std::atomic<uint64_t> notifier{0};
		};

	public:
		thread_pool_executor_lq(std::string name,
								size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency()))
			: executor(std::move(name))
		{
			m_contexts.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				m_contexts.push_back(std::make_unique<thread_context>());
			}

			m_threads.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				std::string worker_name = m_name + "-W" + std::to_string(i);
				m_threads.emplace_back([this, worker_name = std::move(worker_name), i](std::stop_token st)
									   { worker_loop(std::move(st), std::move(worker_name), i); });
			}
		}

		~thread_pool_executor_lq() override
		{
			shutdown();
		}

		void derived_execute(std::function<void()> &&func, pot::coroutines::details::task_meta *meta = nullptr) override
		{
			if (m_stop.load(std::memory_order_acquire))
				throw std::runtime_error("Executor " + m_name + " is stopped.");

			size_t idx = m_next_thread_idx.fetch_add(1, std::memory_order_relaxed) % m_threads.size();
			auto &ctx = *m_contexts[idx];

			{
				std::lock_guard lock(ctx.queue.mtx);
				ctx.queue.tasks.emplace(std::move(func));
			}
			ctx.notifier.fetch_add(1, std::memory_order_release);
			ctx.notifier.notify_one();
		}

		void shutdown() override
		{
			bool expected = false;
			if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
				return;

			for (auto &thread : m_threads)
				thread.request_stop();

			for (auto &ctx : m_contexts)
			{
				ctx->notifier.fetch_add(1, std::memory_order_release);
				ctx->notifier.notify_all();
			}

			m_threads.clear();
		}

		[[nodiscard]] size_t thread_count() const override
		{
			return m_threads.size();
		}

	private:
		void worker_loop(std::stop_token st, std::string name, int64_t local_id)
		{
			pot::this_thread::init_thread_variables(local_id, this);
			pot::this_thread::set_name(name);

			auto &ctx = *m_contexts[local_id];

			while (!st.stop_requested())
			{
				uint64_t wait_val = ctx.notifier.load(std::memory_order_acquire);
				std::function<void()> task;

				{
					std::lock_guard lock(ctx.queue.mtx);
					if (!ctx.queue.tasks.empty())
					{
						task = std::move(ctx.queue.tasks.front());
						ctx.queue.tasks.pop();
					}
				}

				if (task)
				{
					task();
				}
				else
				{
					if (st.stop_requested())
						return;
					ctx.notifier.wait(wait_val, std::memory_order_acquire);
				}
			}
		}

		std::vector<std::jthread> m_threads;
		std::vector<std::unique_ptr<thread_context>> m_contexts;
		std::atomic<size_t> m_next_thread_idx{0};
		std::atomic<bool> m_stop{false};
	};

	class thread_pool_executor_lq_steal_seq final : public executor
	{
		struct thread_context
		{
			pot::work_queue queue;
			std::atomic<uint64_t> notifier{0};
		};

	public:
		thread_pool_executor_lq_steal_seq(std::string name,
										  size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency()))
			: executor(std::move(name))
		{
			m_contexts.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				m_contexts.push_back(std::make_unique<thread_context>());
			}

			m_threads.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				std::string worker_name = m_name + "-W" + std::to_string(i);
				m_threads.emplace_back([this, worker_name = std::move(worker_name), i](std::stop_token st)
									   { worker_loop(std::move(st), std::move(worker_name), i); });
			}
		}

		~thread_pool_executor_lq_steal_seq() override
		{
			shutdown();
		}

		void derived_execute(std::function<void()> &&func, pot::coroutines::details::task_meta *meta = nullptr) override
		{
			if (m_stop.load(std::memory_order_acquire))
				throw std::runtime_error("Executor " + m_name + " is stopped.");

			size_t idx = m_next_thread_idx.fetch_add(1, std::memory_order_relaxed) % m_threads.size();
			auto &ctx = *m_contexts[idx];

			{
				std::lock_guard lock(ctx.queue.mtx);
				ctx.queue.tasks.emplace(std::move(func));
			}
			ctx.notifier.fetch_add(1, std::memory_order_release);
			ctx.notifier.notify_one();
		}

		void shutdown() override
		{
			bool expected = false;
			if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
				return;

			for (auto &thread : m_threads)
				thread.request_stop();

			for (auto &ctx : m_contexts)
			{
				ctx->notifier.fetch_add(1, std::memory_order_release);
				ctx->notifier.notify_all();
			}

			m_threads.clear();
		}

		[[nodiscard]] size_t thread_count() const override
		{
			return m_threads.size();
		}

	private:
		void worker_loop(std::stop_token st, std::string name, int64_t local_id)
		{
			pot::this_thread::init_thread_variables(local_id, this);
			pot::this_thread::set_name(name);

			auto &my_ctx = *m_contexts[local_id];
			size_t num_queues = m_contexts.size();

			while (!st.stop_requested())
			{
				uint64_t wait_val = my_ctx.notifier.load(std::memory_order_acquire);
				std::function<void()> task;

				{
					std::lock_guard lock(my_ctx.queue.mtx);
					if (!my_ctx.queue.tasks.empty())
					{
						task = std::move(my_ctx.queue.tasks.front());
						my_ctx.queue.tasks.pop();
					}
				}

				if (!task && num_queues > 1)
				{
					for (size_t i = 1; i < num_queues; ++i)
					{
						size_t target_idx = (local_id + i) % num_queues;
						auto &target_ctx = *m_contexts[target_idx];

						std::unique_lock lock(target_ctx.queue.mtx, std::try_to_lock);
						if (lock && !target_ctx.queue.tasks.empty())
						{
							task = std::move(target_ctx.queue.tasks.front());
							target_ctx.queue.tasks.pop();
							break;
						}
					}
				}

				if (task)
				{
					task();
				}
				else
				{
					if (st.stop_requested())
						return;
					my_ctx.notifier.wait(wait_val, std::memory_order_acquire);
				}
			}
		}

		std::vector<std::jthread> m_threads;
		std::vector<std::unique_ptr<thread_context>> m_contexts;
		std::atomic<size_t> m_next_thread_idx{0};
		std::atomic<bool> m_stop{false};
	};

	class thread_pool_executor_lq_steal_neighbor final : public executor
	{
		struct thread_context
		{
			pot::work_queue queue;
			std::atomic<uint64_t> notifier{0};
		};

	public:
		thread_pool_executor_lq_steal_neighbor(
			std::string name, size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency()))
			: executor(std::move(name))
		{
			m_contexts.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				m_contexts.push_back(std::make_unique<thread_context>());
			}

			m_threads.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				std::string worker_name = m_name + "-W" + std::to_string(i);
				m_threads.emplace_back([this, worker_name = std::move(worker_name), i](std::stop_token st)
									   { worker_loop(std::move(st), std::move(worker_name), i); });
			}
		}

		~thread_pool_executor_lq_steal_neighbor() override
		{
			shutdown();
		}

		void derived_execute(std::function<void()> &&func, pot::coroutines::details::task_meta *meta = nullptr) override
		{
			if (m_stop.load(std::memory_order_acquire))
				throw std::runtime_error("Executor " + m_name + " is stopped.");

			size_t idx = m_next_thread_idx.fetch_add(1, std::memory_order_relaxed) % m_threads.size();
			auto &ctx = *m_contexts[idx];

			{
				std::lock_guard lock(ctx.queue.mtx);
				ctx.queue.tasks.emplace(std::move(func));
			}
			ctx.notifier.fetch_add(1, std::memory_order_release);
			ctx.notifier.notify_one();
		}

		void shutdown() override
		{
			bool expected = false;
			if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
				return;

			for (auto &thread : m_threads)
				thread.request_stop();

			for (auto &ctx : m_contexts)
			{
				ctx->notifier.fetch_add(1, std::memory_order_release);
				ctx->notifier.notify_all();
			}

			m_threads.clear();
		}

		[[nodiscard]] size_t thread_count() const override
		{
			return m_threads.size();
		}

	private:
		void worker_loop(std::stop_token st, std::string name, int64_t local_id)
		{
			pot::this_thread::init_thread_variables(local_id, this);
			pot::this_thread::set_name(name);

			auto &my_ctx = *m_contexts[local_id];
			size_t num_queues = m_contexts.size();

			while (!st.stop_requested())
			{
				uint64_t wait_val = my_ctx.notifier.load(std::memory_order_acquire);
				std::function<void()> task;

				{
					std::lock_guard lock(my_ctx.queue.mtx);
					if (!my_ctx.queue.tasks.empty())
					{
						task = std::move(my_ctx.queue.tasks.front());
						my_ctx.queue.tasks.pop();
					}
				}

				if (!task && num_queues > 1)
				{
					size_t left = (local_id + num_queues - 1) % num_queues;
					size_t right = (local_id + 1) % num_queues;

					for (size_t target_idx : {right, left})
					{
						auto &target_ctx = *m_contexts[target_idx];
						std::unique_lock lock(target_ctx.queue.mtx, std::try_to_lock);
						if (lock && !target_ctx.queue.tasks.empty())
						{
							task = std::move(target_ctx.queue.tasks.front());
							target_ctx.queue.tasks.pop();
							break;
						}
					}
				}

				if (task)
				{
					task();
				}
				else
				{
					if (st.stop_requested())
						return;
					my_ctx.notifier.wait(wait_val, std::memory_order_acquire);
				}
			}
		}

		std::vector<std::jthread> m_threads;
		std::vector<std::unique_ptr<thread_context>> m_contexts;
		std::atomic<size_t> m_next_thread_idx{0};
		std::atomic<bool> m_stop{false};
	};

	class thread_pool_executor_gs final : public executor
	{
	public:
		thread_pool_executor_gs(std::string name,
								size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency()))
			: executor(std::move(name))
		{
			m_threads.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				std::string worker_name = m_name + "-W" + std::to_string(i);
				m_threads.emplace_back([this, worker_name = std::move(worker_name), i](std::stop_token st)
									   { worker_loop(std::move(st), std::move(worker_name), i); });
			}
		}

		~thread_pool_executor_gs() override
		{
			shutdown();
		}

		void derived_execute(std::function<void()> &&func, pot::coroutines::details::task_meta *meta = nullptr) override
		{
			if (m_stop.load(std::memory_order_acquire))
				throw std::runtime_error("Executor " + m_name + " is stopped.");

			{
				std::lock_guard lock(m_stack.mtx);
				m_stack.tasks.emplace(std::move(func));
			}
			m_notifier.fetch_add(1, std::memory_order_release);
			m_notifier.notify_one();
		}

		void shutdown() override
		{
			bool expected = false;
			if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
				return;

			for (auto &thread : m_threads)
				thread.request_stop();

			m_notifier.fetch_add(m_threads.size(), std::memory_order_release);
			m_notifier.notify_all();

			m_threads.clear();
		}

		[[nodiscard]] size_t thread_count() const override
		{
			return m_threads.size();
		}

	private:
		void worker_loop(std::stop_token st, std::string name, int64_t local_id)
		{
			pot::this_thread::init_thread_variables(local_id, this);
			pot::this_thread::set_name(name);

			while (!st.stop_requested())
			{
				uint64_t wait_val = m_notifier.load(std::memory_order_acquire);
				std::function<void()> task;

				{
					std::lock_guard lock(m_stack.mtx);
					if (!m_stack.tasks.empty())
					{
						task = std::move(m_stack.tasks.top());
						m_stack.tasks.pop();
					}
				}

				if (task)
				{
					task();
				}
				else
				{
					if (st.stop_requested())
						return;
					m_notifier.wait(wait_val, std::memory_order_acquire);
				}
			}
		}

		std::vector<std::jthread> m_threads;
		pot::work_stack m_stack;
		std::atomic<uint64_t> m_notifier{0};
		std::atomic<bool> m_stop{false};
	};

	class thread_pool_executor_ls : public executor
	{
		struct thread_context
		{
			pot::work_stack stack;
			std::atomic<uint64_t> notifier{0};
		};

	public:
		thread_pool_executor_ls(std::string name,
								size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency()))
			: executor(std::move(name))
		{
			m_contexts.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				m_contexts.push_back(std::make_unique<thread_context>());
			}

			m_threads.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				std::string worker_name = m_name + "-W" + std::to_string(i);
				m_threads.emplace_back([this, worker_name = std::move(worker_name), i](std::stop_token st)
									   { worker_loop(std::move(st), std::move(worker_name), i); });
			}
		}

		~thread_pool_executor_ls() override
		{
			shutdown();
		}

		void derived_execute(std::function<void()> &&func, pot::coroutines::details::task_meta *meta = nullptr) override
		{
			if (m_stop.load(std::memory_order_acquire))
				throw std::runtime_error("Executor " + m_name + " is stopped.");

			size_t idx = m_next_thread_idx.fetch_add(1, std::memory_order_relaxed) % m_threads.size();
			auto &ctx = *m_contexts[idx];

			{
				std::lock_guard lock(ctx.stack.mtx);
				ctx.stack.tasks.emplace(std::move(func));
			}
			ctx.notifier.fetch_add(1, std::memory_order_release);
			ctx.notifier.notify_one();
		}

		void shutdown() override
		{
			bool expected = false;
			if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
				return;

			for (auto &thread : m_threads)
				thread.request_stop();

			for (auto &ctx : m_contexts)
			{
				ctx->notifier.fetch_add(1, std::memory_order_release);
				ctx->notifier.notify_all();
			}

			m_threads.clear();
		}

		[[nodiscard]] size_t thread_count() const override
		{
			return m_threads.size();
		}

	private:
		void worker_loop(std::stop_token st, std::string name, int64_t local_id)
		{
			pot::this_thread::init_thread_variables(local_id, this);
			pot::this_thread::set_name(name);

			auto &ctx = *m_contexts[local_id];

			while (!st.stop_requested())
			{
				uint64_t wait_val = ctx.notifier.load(std::memory_order_acquire);
				std::function<void()> task;

				{
					std::lock_guard lock(ctx.stack.mtx);
					if (!ctx.stack.tasks.empty())
					{
						task = std::move(ctx.stack.tasks.top());
						ctx.stack.tasks.pop();
					}
				}

				if (task)
				{
					task();
				}
				else
				{
					if (st.stop_requested())
						return;
					ctx.notifier.wait(wait_val, std::memory_order_acquire);
				}
			}
		}

		std::vector<std::jthread> m_threads;
		std::vector<std::unique_ptr<thread_context>> m_contexts;
		std::atomic<size_t> m_next_thread_idx{0};
		std::atomic<bool> m_stop{false};
	};

	class thread_pool_executor_gs_hot final : public executor
	{
	public:
		thread_pool_executor_gs_hot(std::string name,
									size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency()))
			: executor(std::move(name))
		{
			m_threads.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				std::string worker_name = m_name + "-W" + std::to_string(i);
				m_threads.emplace_back([this, worker_name = std::move(worker_name), i](std::stop_token st)
									   { worker_loop(std::move(st), std::move(worker_name), i); });
			}
		}

		~thread_pool_executor_gs_hot() override
		{
			shutdown();
		}

		void derived_execute(std::function<void()> &&func, pot::coroutines::details::task_meta *meta = nullptr) override
		{
			if (m_stop.load(std::memory_order_acquire))
				throw std::runtime_error("Executor " + m_name + " is stopped.");

			size_t runs = meta ? meta->run_count.load(std::memory_order_relaxed) : 0;

			{
				std::lock_guard lock(m_deque.mtx);

				if (runs <= 10)
				{
					m_deque.tasks.emplace_front(std::move(func));
				}
				else
				{
					m_deque.tasks.emplace_back(std::move(func));
				}
			}

			m_notifier.fetch_add(1, std::memory_order_release);
			m_notifier.notify_one();
		}

		void shutdown() override
		{
			bool expected = false;
			if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
				return;

			for (auto &thread : m_threads)
				thread.request_stop();

			m_notifier.fetch_add(m_threads.size(), std::memory_order_release);
			m_notifier.notify_all();

			m_threads.clear();
		}

		[[nodiscard]] size_t thread_count() const override
		{
			return m_threads.size();
		}

	private:
		void worker_loop(std::stop_token st, std::string name, int64_t local_id)
		{
			pot::this_thread::init_thread_variables(local_id, this);
			pot::this_thread::set_name(name);

			while (!st.stop_requested())
			{
				uint64_t wait_val = m_notifier.load(std::memory_order_acquire);
				std::function<void()> task;

				{
					std::lock_guard lock(m_deque.mtx);
					if (!m_deque.tasks.empty())
					{
						task = std::move(m_deque.tasks.front());
						m_deque.tasks.pop_front();
					}
				}

				if (task)
				{
					task();
				}
				else
				{
					if (st.stop_requested())
						return;
					m_notifier.wait(wait_val, std::memory_order_acquire);
				}
			}
		}

		std::vector<std::jthread> m_threads;
		pot::work_deque m_deque;
		std::atomic<uint64_t> m_notifier{0};
		std::atomic<bool> m_stop{false};
	};

	class thread_pool_executor_ls_hot final : public executor
	{
		struct thread_context
		{
			pot::work_deque deque;
			std::atomic<uint64_t> notifier{0};
		};

	public:
		thread_pool_executor_ls_hot(std::string name,
									size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency()))
			: executor(std::move(name))
		{

			m_contexts.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				m_contexts.push_back(std::make_unique<thread_context>());
			}

			m_threads.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				std::string worker_name = m_name + "-W" + std::to_string(i);
				m_threads.emplace_back([this, worker_name = std::move(worker_name), i](std::stop_token st)
									   { worker_loop(std::move(st), std::move(worker_name), i); });
			}
		}

		~thread_pool_executor_ls_hot() override
		{
			shutdown();
		}

		void derived_execute(std::function<void()> &&func, pot::coroutines::details::task_meta *meta = nullptr) override
		{
			if (m_stop.load(std::memory_order_acquire))
				throw std::runtime_error("Executor " + m_name + " is stopped.");

			size_t runs = meta ? meta->run_count.load(std::memory_order_relaxed) : 0;

			size_t idx = m_next_thread_idx.fetch_add(1, std::memory_order_relaxed) % m_threads.size();
			auto &ctx = *m_contexts[idx];

			{
				std::lock_guard lock(ctx.deque.mtx);

				if (runs <= 10)
				{

					ctx.deque.tasks.emplace_front(std::move(func));
				}
				else
				{

					ctx.deque.tasks.emplace_back(std::move(func));
				}
			}

			ctx.notifier.fetch_add(1, std::memory_order_release);
			ctx.notifier.notify_one();
		}

		void shutdown() override
		{
			bool expected = false;
			if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
				return;

			for (auto &thread : m_threads)
				thread.request_stop();

			for (auto &ctx : m_contexts)
			{
				ctx->notifier.fetch_add(1, std::memory_order_release);
				ctx->notifier.notify_all();
			}

			m_threads.clear();
		}

		[[nodiscard]] size_t thread_count() const override
		{
			return m_threads.size();
		}

	private:
		void worker_loop(std::stop_token st, std::string name, int64_t local_id)
		{
			pot::this_thread::init_thread_variables(local_id, this);
			pot::this_thread::set_name(name);

			auto &ctx = *m_contexts[local_id];

			while (!st.stop_requested())
			{
				uint64_t wait_val = ctx.notifier.load(std::memory_order_acquire);
				std::function<void()> task;

				{
					std::lock_guard lock(ctx.deque.mtx);
					if (!ctx.deque.tasks.empty())
					{
						task = std::move(ctx.deque.tasks.front());
						ctx.deque.tasks.pop_front();
					}
				}

				if (task)
				{
					task();
				}
				else
				{
					if (st.stop_requested())
						return;
					ctx.notifier.wait(wait_val, std::memory_order_acquire);
				}
			}
		}

		std::vector<std::jthread> m_threads;
		std::vector<std::unique_ptr<thread_context>> m_contexts;
		std::atomic<size_t> m_next_thread_idx{0};
		std::atomic<bool> m_stop{false};
	};

	class thread_pool_executor_gpq final : public executor
	{
		struct task_item
		{
			std::function<void()> func;
			uint64_t priority;
			uint64_t sequence;

			bool operator<(const task_item &other) const
			{
				if (priority == other.priority)
				{
					return sequence > other.sequence;
				}
				return priority > other.priority;
			}
		};

		struct work_pqueue
		{
			std::priority_queue<task_item> tasks;
			std::mutex mtx;
			uint64_t seq_counter{0};
		};

	public:
		thread_pool_executor_gpq(std::string name,
								 size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency()))
			: executor(std::move(name))
		{
			m_threads.reserve(num_threads);
			for (size_t i = 0; i < num_threads; ++i)
			{
				std::string worker_name = m_name + "-W" + std::to_string(i);
				m_threads.emplace_back([this, worker_name = std::move(worker_name), i](std::stop_token st)
									   { worker_loop(std::move(st), std::move(worker_name), i); });
			}
		}

		~thread_pool_executor_gpq() override
		{
			shutdown();
		}

		void derived_execute(std::function<void()> &&func, pot::coroutines::details::task_meta *meta = nullptr) override
		{
			if (m_stop.load(std::memory_order_acquire))
				throw std::runtime_error("Executor " + m_name + " is stopped.");

			size_t priority = meta ? meta->run_count.load(std::memory_order_relaxed) : 0;

			{
				std::lock_guard lock(m_queue.mtx);
				m_queue.tasks.push(task_item{std::move(func), priority, m_queue.seq_counter++});
			}

			m_notifier.fetch_add(1, std::memory_order_release);
			m_notifier.notify_one();
		}

		void shutdown() override
		{
			bool expected = false;
			if (!m_stop.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
				return;

			for (auto &thread : m_threads)
				thread.request_stop();

			m_notifier.fetch_add(m_threads.size(), std::memory_order_release);
			m_notifier.notify_all();

			m_threads.clear();
		}

		[[nodiscard]] size_t thread_count() const override
		{
			return m_threads.size();
		}

	private:
		void worker_loop(std::stop_token st, std::string name, int64_t local_id)
		{
			pot::this_thread::init_thread_variables(local_id, this);
			pot::this_thread::set_name(name);

			while (!st.stop_requested())
			{
				uint64_t wait_val = m_notifier.load(std::memory_order_acquire);
				std::function<void()> task;

				{
					std::lock_guard lock(m_queue.mtx);
					if (!m_queue.tasks.empty())
					{
						task = std::move(m_queue.tasks.top().func);
						m_queue.tasks.pop();
					}
				}

				if (task)
				{
					task();
				}
				else
				{
					if (st.stop_requested())
						return;
					m_notifier.wait(wait_val, std::memory_order_acquire);
				}
			}
		}

		std::vector<std::jthread> m_threads;
		work_pqueue m_queue;
		std::atomic<uint64_t> m_notifier{0};
		std::atomic<bool> m_stop{false};
	};
} // namespace pot::executors
