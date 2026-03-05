#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "pot/coroutines/task.h"
#include "pot/utils/time_it.h"
#include "pot/utils/platform.h"

namespace pot
{
	class executor
	{
	protected:
		std::string m_name;

		virtual void derived_execute(std::function<void()> &&func,
									 pot::coroutines::details::task_meta *meta = nullptr) = 0;

	public:
		explicit executor(std::string name) : m_name(std::move(name))
		{
		}
		virtual ~executor() = default;

		executor(const executor &) = delete;
		executor &operator=(const executor &) = delete;

		executor(executor &&) = default;
		executor &operator=(executor &&) = default;

		[[nodiscard]] std::string name() const
		{
			return m_name;
		}

		template <typename Func> void run_detached_meta(Func &&func, pot::coroutines::details::task_meta *meta)
		{
			derived_execute(std::forward<Func>(func), meta);
		}

		template <typename Func, typename... Args>
			requires std::is_invocable_v<Func, Args...>
		void run_detached(Func &&func, Args &&...args)
		{
			derived_execute([f = std::decay_t<Func>(std::forward<Func>(func)),
							 ... captured_args = std::decay_t<Args>(std::forward<Args>(args))]() mutable
							{ std::invoke(f, std::move(captured_args)...); });
		}

		template <typename Func, typename... Args>
		auto run(Func &&func, Args &&...args)
			-> pot::coroutines::task<pot::traits::awaitable_value_t<std::invoke_result_t<Func, Args...>>>
		{
			return do_run<false>(std::forward<Func>(func), std::forward<Args>(args)...);
		}

		template <typename Func, typename... Args>
		auto lazy_run(Func &&func, Args &&...args)
			-> pot::coroutines::lazy_task<pot::traits::awaitable_value_t<std::invoke_result_t<Func, Args...>>>
		{
			return do_run<true>(std::forward<Func>(func), std::forward<Args>(args)...);
		}

		virtual void shutdown() = 0;
		[[nodiscard]] virtual size_t thread_count() const
		{
			return 1;
		}

		static void cpu_set()
		{
			unsigned int num_cores = std::thread::hardware_concurrency();

			std::vector<std::pair<int, long long>> results;
			results.reserve(num_cores);

#ifdef POT_PLATFORM_LINUX
			cpu_set_t original_cpuset;
			CPU_ZERO(&original_cpuset);
			pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &original_cpuset);
#endif

			for (unsigned int i = 0; i < num_cores; ++i)
			{
#ifdef POT_PLATFORM_LINUX
				cpu_set_t cpuset;
				CPU_ZERO(&cpuset);
				CPU_SET(static_cast<size_t>(i), &cpuset);

				if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) 
					continue;
#endif

				volatile double val = 0.0;
				for (int k = 0; k < 100'000; ++k) val += 1.0;

				auto duration = pot::utils::time_it<std::chrono::nanoseconds>(10, []{}, [] 
				{
					volatile double val = 1.0;
					for (int k = 0; k < 100'000'000; ++k) val = (val + 1.5) * 0.5;
				});

				results.emplace_back(static_cast<int>(i), duration.count());
			}

#ifdef POT_PLATFORM_LINUX
			pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &original_cpuset);
#endif

			std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) 
			{
				return a.second > b.second; 
			});

			std::vector<int> sorted_cores;
			sorted_cores.reserve(results.size());
			for (const auto& res : results) 
			{
				sorted_cores.push_back(res.first);
			}

			pot::coroutines::details::task_meta::sorted_cpu_list = std::move(sorted_cores);
		}

		virtual bool try_steal() { return false; }

	private:
		template <bool Lazy, typename Func, typename... Args>

		auto do_run(Func func, Args... args) -> std::conditional_t<
			Lazy, pot::coroutines::lazy_task<pot::traits::awaitable_value_t<std::invoke_result_t<Func, Args...>>>,
			pot::coroutines::task<pot::traits::awaitable_value_t<std::invoke_result_t<Func, Args...>>>>
		{
			using Ret = std::invoke_result_t<Func, Args...>;
			using AwaitableValue = pot::traits::awaitable_value_t<Ret>;

			using TaskType = std::conditional_t<Lazy, pot::coroutines::lazy_task<AwaitableValue>,
												pot::coroutines::task<AwaitableValue>>;

			using PromiseType = typename TaskType::promise_type;

			struct schedule_awaiter
			{
				executor *exec;
				bool await_ready() const noexcept
				{
					return false;
				}

				void await_suspend(std::coroutine_handle<void> h)
				{
					auto typed_handle = std::coroutine_handle<PromiseType>::from_address(h.address());
					typed_handle.promise().meta.run_count.fetch_add(1, std::memory_order_relaxed);
					exec->derived_execute([h]() mutable { h.resume(); }, &typed_handle.promise().meta);
				}

				void await_resume() const noexcept
				{
				}
			};

			co_await schedule_awaiter{this};

			if constexpr (std::is_void_v<Ret>)
			{
				std::invoke(std::move(func), std::move(args)...);
				co_return;
			}
			else
			{
				if constexpr (pot::traits::is_task_v<Ret> || pot::traits::is_lazy_task_v<Ret>)
				{
					co_return co_await std::invoke(std::move(func), std::move(args)...);
				}
				else
				{
					co_return std::invoke(std::move(func), std::move(args)...);
				}
			}
		}
	};
} // namespace pot
