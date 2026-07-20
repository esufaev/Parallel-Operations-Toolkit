# Parallel Operations Toolkit

C++20 header-only library for parallel computing: coroutines, executors, SIMD abstractions, lock-free data structures, and parallel algorithms.

[Русская версия](README.ru.md)

## Installation

Add the library to your project using **CMake** and `FetchContent`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(MY_PROJECT LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
  pot
  GIT_REPOSITORY https://github.com/esufaev/Parallel-Operations-Toolkit.git
  GIT_TAG master
)

FetchContent_MakeAvailable(pot)

add_executable(MY_PROJECT main.cpp)
target_link_libraries(MY_PROJECT PRIVATE pot::pot)
```

To include everything at once:
```cpp
#include "pot/pot.h"
```

---

## Coroutines

### Task (eager)

Starts execution immediately upon creation.

```cpp
#include "pot/coroutines/task.h"

pot::coroutines::task<int> compute() { co_return 42; }

int main() {
    auto t = compute();
    std::cout << "Result: " << t.get() << "\n";
}
```

### Lazy Task

Nothing executes until `get()` is called.

```cpp
#include "pot/coroutines/task.h"

pot::coroutines::lazy_task<int> delayed() {
    std::cout << "Started computation\n";
    co_return 7;
}

int main() {
    auto lt = delayed();       // nothing happens
    std::cout << "Before get()\n";
    std::cout << "Value: " << lt.get() << "\n";  // launches the coroutine
}
```

Output:

`Before get() Started computation Value: 7`

---

## Executors

Executors (`executor`) provide an abstraction for running functions or coroutines on different execution strategies.

All executors implement the `pot::executor` interface and can be used interchangeably.

### `pot::executor` (base class)

- `run_detached(func, args...)` — fire and forget.
- `run(func, args...)` — run and return an eager `task`.
- `lazy_run(func, args...)` — run and return a `lazy_task`.
- `shutdown()` — stop the executor.
- `thread_count()` — number of worker threads.

---

### `pot::executors::inline_executor`

Runs tasks **synchronously** in the current thread. Useful for testing and debugging.

```cpp
pot::executors::inline_executor exec("inline");
exec.run_detached([] {
    std::cout << "Runs in the same thread" << std::endl;
});
```

---

### `pot::executors::thread_executor`

Allocates a **dedicated thread** for all submitted tasks.

```cpp
pot::executors::thread_executor exec("single-thread");
exec.run_detached([] {
    std::cout << "Runs in the dedicated thread" << std::endl;
});
exec.shutdown();
```

---

### Thread Pool Executors

The library provides several thread pool strategies, differing in queue structure and load balancing:

| Class | Queue | Topology | Work stealing |
|---|---|---|---|
| `thread_pool_executor_gq` | `std::queue` + mutex | Global | No |
| `thread_pool_executor_lq` | `std::queue` + mutex | Local (round-robin) | No |
| `thread_pool_executor_lfgq` | Lock-free queue | Global | No |
| `thread_pool_executor_lflq` | Lock-free queue | Local (round-robin) | No |

All pools default to `std::thread::hardware_concurrency()` threads.

```cpp
pot::executors::thread_pool_executor_lfgq pool("pool", 12);
for (int i = 0; i < 10; ++i) {
    pool.run_detached([i] {
        std::cout << "Task " << i << " in thread pool" << std::endl;
    });
}
pool.shutdown();
```

---

## Parallel Algorithms

### Parfor

Asynchronous parallel `for` loop. Automatically splits the iteration range into **chunks** and executes them on the given executor.

#### Signature

```cpp
template <int64_t static_chunk_size = -1, typename IndexType, typename FuncType = void(IndexType)>
  requires std::invocable<FuncType &, IndexType>
pot::coroutines::lazy_task<void>
parfor(pot::executor &executor, IndexType from, IndexType to, FuncType&& func);
```

#### Parameters

| Parameter | Type | Description |
|---|---|---|
| `static_chunk_size` | `int64_t` (default `-1`) | Chunk size. If `< 0`, computed automatically. |
| `executor` | `pot::executor&` | Executor for parallelization. |
| `from` | `IndexType` | Start index (inclusive). |
| `to` | `IndexType` | End index (exclusive). |
| `func` | callable | Function for each iteration. Supports both synchronous and coroutine functions. |

#### Return value

`pot::coroutines::lazy_task<void>` — completes when all parallel tasks finish.

#### Examples

**Synchronous function:**

```cpp
#include "pot/algorithms/parfor.h"
#include "pot/executors/thread_pool_executor.h"

void example_sync() {
    pot::thread_pool_executor exec(4);

    parfor(exec, 0, 100, [](int i) {
        printf("Index: %d\n", i);
    }).get();
}
```

**Asynchronous function (coroutine):**

```cpp
pot::coroutines::task<void> process_item(int i) {
    co_await some_async_operation(i);
}

void example_async() {
    pot::thread_pool_executor exec(4);

    parfor(exec, 0, 100, [](int i) -> pot::coroutines::task<void> {
        co_await process_item(i);
    }).sync_wait();
}
```

---

### Parsections

Runs multiple independent sections in parallel. Completes when **all** sections finish.

```cpp
pot::coroutines::task<void> coroA();
pot::coroutines::lazy_task<void> coroB();

co_await pot::algorithms::parsections(exec,
    []                                       { prepare();                   },
    []() -> pot::coroutines::task<void>      { co_await coroA(); co_return; },
    []() -> pot::coroutines::lazy_task<void> { co_await coroB(); co_return; }
);
```

---

### When_all

Combinator that completes when **all** provided awaitables finish.

```cpp
// Variadic
co_await pot::coroutines::when_all(a(), b(), c());

// Container
std::vector<pot::coroutines::task<void>> tasks;
tasks.push_back(do_work(1));
tasks.push_back(do_work(2));
co_await pot::coroutines::when_all(tasks);

// Iterator range
co_await pot::coroutines::when_all(tasks.begin(), tasks.end());
```

---

### Elementwise_reduce

Asynchronous element-wise reduction over two arrays. Applies a binary operation (`elem_op(a[i], b[i])`) to each pair, then reduces the results.

```cpp
// Dot product
co_return co_await pot::algorithms::elementwise_reduce<double, double>(
    exec, a, b,
    std::multiplies<double>{},
    std::plus<double>{},
    0.0
);
```

### Elementwise_reduce_simd

SIMD variant: processes multiple elements per iteration using `simd_forced`, then reduces the tail scalarily.

```cpp
template <typename T, pot::simd::SIMDType ST>
pot::coroutines::lazy_task<T>
l1_simd(pot::executor& exec, std::span<const T> a, std::span<const T> b) {
    auto simd_abs_diff = [](auto va, auto vb) {
        auto vd = va - vb;
        return vd.abs();
    };
    auto scalar_abs_diff = [](T x, T y) { return std::abs(x - y); };

    co_return co_await pot::algorithms::elementwise_reduce_simd<T, T, ST>(
        exec, a, b, simd_abs_diff, scalar_abs_diff, std::plus<T>{}, T{0});
}
```

### Dot / Dot_simd

Asynchronous dot product of two arrays.

```cpp
// Regular version
auto res = co_await pot::algorithms::dot(exec, a, b);

// SIMD version
co_return co_await pot::algorithms::dot_simd<float, pot::simd::SIMDType::AVX>(exec, a, b);
```

---

## Synchronization

### Async Condition Variable

Asynchronous condition variable for coroutines. **Does not block the thread** — suspends the coroutine until signaled.

```cpp
pot::coroutines::async_condition_variable cv;

pot::coroutines::task<void> waiter(int id) {
    co_await cv;
    std::cout << "Coroutine " << id << " resumed!" << std::endl;
}

pot::coroutines::task<void> example() {
    auto task1 = waiter(1);
    auto task2 = waiter(2);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    cv.set();  // resumes all waiters
}
```

| Method | Description |
|---|---|
| `async_condition_variable(bool set = false)` | Constructor. Optional initial state. |
| `operator co_await()` | Wait for signal. |
| `set()` | Set flag and resume all waiters. |
| `stop()` | Clear flag and discard waiters without resuming. |
| `is_set()` | Check if flag is set. |
| `reset()` | Clear flag. Waiters are not removed. |

---

### Async Barrier

Asynchronous barrier for coroutines. All coroutines suspend until `set()` has been called the required number of times.

```cpp
pot::coroutines::async_barrier barrier(3);  // wait for 3 set() calls

pot::coroutines::task<void> worker(int id) {
    co_await barrier;  // waits until all 3 workers call set()
    std::cout << "Worker " << id << " passed the barrier" << std::endl;
}

// Caller:
co_await worker(1);
co_await worker(2);
co_await worker(3);
// All three complete → barrier lets them through
```

---

### Async Lock

Asynchronous lock for coroutines. Does not block the thread — suspends the coroutine until the lock is acquired.

```cpp
pot::sync::async_lock lock;
pot::executors::thread_pool_executor exec(4);

pot::coroutines::task<void> critical_section() {
    auto guard = co_await lock.lock(&exec);
    // guard — scoped_lock_guard, automatically calls unlock on scope exit
    do_shared_work();
    // unlock happens automatically
}
```

---

### Sync Object

Thread-safe wrapper around an object. Accessing via `->` or `*` automatically acquires a mutex.

```cpp
pot::sync::sync_object<std::vector<int>> safe_vec(std::vector<int>{1, 2, 3});

{
    auto locked = safe_vec.scoped();  // std::scoped_lock
    locked->push_back(4);            // thread-safe
}
```

---

## Resume_on

Returns an awaitable that resumes the current coroutine on a given `executor`.

```cpp
using pot::coroutines::resume_on;

pot::coroutines::task<void> do_work(pot::executor& cpu1, pot::executor& cpu2) {
    co_await resume_on(cpu1);
    co_await heavy_compute();    // on CPU1
    co_await resume_on(cpu2);
    update();                    // on CPU2
}
```

---

## SIMD

The library provides two SIMD classes: **`simd_forced`** (uses hardware intrinsics) and **`simd_auto`** (scalar fallback).

### `pot::simd::simd_forced<T, ST>`

Forced SIMD execution for type `T` and instruction set `ST`.

```cpp
pot::simd::simd_forced<float, pot::simd::SIMDType::AVX> a(1.0f);
pot::simd::simd_forced<float, pot::simd::SIMDType::AVX> b(2.0f);

auto c = a + b;     // AVX addition
auto d = a * b;     // AVX multiplication
auto e = a.abs();   // AVX absolute value
```

**Supported `T`:** `int8_t`, `uint8_t`, `int16_t`, `uint16_t`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `float`, `double`.

**Supported `SIMDType`:**

| `SIMDType` | Register size | `float` count | `double` count |
|---|---|---|---|
| `SSE` | 128-bit | 4 | 2 |
| `AVX` | 256-bit | 8 | 4 |
| `AVX512` | 512-bit | 16 | 8 |

**Operations:**

Arithmetic: `+`, `-`, `*`, `/`, `%`, unary `-`, `+`, `++`, `--`, `+=`, `-=`, `*=`, `/=`, `%=`

Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`

Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=` (return `bool`)

Math: `abs`, `sqrt`, `sqr`, `sum`, `prod`, `exp`, `log`, `log2`, `log10`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`, `ceil`, `floor`, `trunc`, `round`, `min`, `max`

### `pot::simd::simd_auto<T, N>`

Scalar implementation with the same interface. Useful as a fallback when SIMD instructions are unavailable, or for portability.

```cpp
pot::simd::simd_auto<float, 8> a(1.0f);
pot::simd::simd_auto<float, 8> b(2.0f);
auto c = a + b;  // scalar addition
```

---

## Lock-free Data Structures

### LFQueue (lock-free MPSC queue)

Multi-producer single-consumer queue backed by a ring buffer.

```cpp
pot::algorithms::lfqueue<int> queue;

// Producer:
queue.push(42);

// Consumer:
auto val = queue.pop();
if (val) {
    std::cout << "Received: " << *val << std::endl;
}
```

### Orbit MPMC Queue

Multi-producer multi-consumer queue with tunable performance parameters.

```cpp
orbit::mpmc_queue<int, 1024, true, true> queue;  // MINIMISE_LATENCY=true, NONBLOCKING=true

queue.push(42);
auto val = queue.pop();          // blocking pop
bool ok = queue.try_pop(val);    // non-blocking pop
```

**Template parameters:**

| Parameter | Default | Description |
|---|---|---|
| `SIZE` | — | Buffer size (must be a power of two). |
| `MINIMISE_LATENCY` | `true` | Optimize for latency (`false` — throughput). |
| `NONBLOCKING` | `true` | `true` — lock-free, `false` — removes CAS for lower latency. |
| `PAUSE_SHORT` | `3` | Spin-loop pause count. |
| `PAUSE_LONG` | `40` | Pause in throughput mode. |

---

## Utilities

### Time It

Measure function execution time.

```cpp
// Single measurement
auto duration = pot::utils::time_it<std::chrono::milliseconds>([] {
    do_work();
});

// Average over N runs
auto avg = pot::utils::time_it<std::chrono::microseconds>(100, [] {
    cleanup();
}, [] {
    do_work();
});
```

### Unique Function Once

One-shot callable with SBO optimization (64 bytes).

```cpp
pot::utils::unique_function_once f([] { std::cout << "called\n"; });
f();    // executes and is destroyed
// f(); // UB — calling twice
```

### Function (PMR-aware)

`std::function` replacement with PMR allocator support and SBO.

```cpp
pot::utils::function<int(int, int)> add = [](int a, int b) { return a + b; };
int result = add(2, 3);  // 5

// With custom allocator
pot::utils::function<void()> f(std::allocator_arg, my_resource, [] { ... });
```

### Cache Line

Cache line alignment constant.

```cpp
constexpr std::size_t alignment = pot::cache_line_alignment;  // typically 64
```

### This Thread

Thread utilities: names, identifiers, priorities.

```cpp
pot::this_thread::set_name("Worker");
auto name = pot::this_thread::name();
auto sys_id = pot::this_thread::system_id();
auto local_id = pot::this_thread::local_id();

pot::this_thread::set_params(SCHED_FIFO, 10);  // scheduling policy and priority
```

### Platform

Compile-time platform and compiler detection.

```cpp
constexpr auto os = pot::platform::current_OS;       // Linux, Windows, MacOS, ...
constexpr auto compiler = pot::platform::current_сompiler;  // GCC, Clang, MSVC, ...
```

### Progress

Atomic progress tracker for parallel tasks.

```cpp
pot::coroutines::details::progress p;
p.set_progress_range(0, 100);
p.set_progress_value(50);
p.set_progress_value_and_text(75, "Almost done...");
bool done = p.is_finished();
```
