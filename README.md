# Parallel Operations Toolkit

Этот модуль предоставляет удобные обёртки над C++20 корутинами:

## 🚀 Установка

Добавьте библиотеку в ваш проект с помощью **CMake** и `FetchContent`:

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

## Task

### Немедленная задача (`task`)

```cpp
#include <iostream> 
#include "pot/coroutines/task.h"  

pot::coroutines::task<int> compute() { co_return 42; }  

int main() {     
	auto t = compute(); // немедленно выполняется
	std::cout << "Result: " << t.get() << "\n"; }
```

### Ленивая задача (`lazy_task`)

```cpp
#include <iostream> 
#include "pot/coroutines/task.h"  

pot::coroutines::lazy_task<int> delayed() {     
	std::cout << "Started computation\n";     
	co_return 7; 
}  
	
int main() {     
	auto lt = delayed(); // ничего не выполняется     
	std::cout << "Before get()\n";     
	std::cout << "Value: " << lt.get() << "\n"; // запускает корутину 
}
```

Вывод:

`Before get() Started computation Value: 7`

## Executor

Исполнители (`executor`) — это абстракция, позволяющая запускать функции или корутины на различных стратегиях выполнения:

- **в текущем потоке** (`inline_executor`)
    
- **в выделенном отдельном потоке** (`thread_executor`)
    
- **в пуле потоков** (`thread_pool_executor`)
    

Все исполнители реализуют единый интерфейс `pot::executor`, что позволяет использовать их взаимозаменяемо.

---

### `pot::executor` (базовый класс)


Абстрактный базовый класс, определяющий контракт для запуска задач. Конкретные исполнители реализуют метод `derived_execute`, который отвечает за фактический запуск переданной функции.

- `run_detached(func, args...)` — запустить задачу без ожидания результата.
    
- `run(func, args...)` — запустить задачу и вернуть `task` (асинхронный результат).
    
- `lazy_run(func, args...)` — запустить задачу и вернуть `lazy_task` (ленивое выполнение).
    
- `shutdown()` — завершить работу исполнителя.
    
- `thread_count()` — количество рабочих потоков (по умолчанию `1`).
    

---

### `pot::executors::inline_executor`

Запускает задачи **синхронно** в текущем потоке.  
Полезно для тестов, отладки или случаев, когда многопоточность не нужна.
Метод `derived_execute` просто вызывает переданную функцию в текущем потоке.

```cpp 
pot::executors::inline_executor exec("inline");  
exec.run_detached([] 
{     
	std::cout << "Выполняется в том же потоке" << std::endl; 
});
```

---

### `pot::executors::thread_executor`

Выделяет **один отдельный поток**, в котором будут выполняться все задачи.

### Пример использования

```cpp
pot::executors::thread_executor exec("single-thread");  
exec.run_detached([] 
{     
	std::cout << "Выполняется в выделенном потоке" << std::endl; 
});  
exec.shutdown();
```

---

### `pot::executors::thread_pool_executor_lfgq`

Пул потоков с **lock-free очередью** (`lfqueue`). Количество потоков задаётся в конструкторе. По умолчанию — `std::thread::hardware_concurrency()`.

### Пример использования

```cpp
pot::executors::thread_pool_executor_lfgq pool("pool", 12);  
for (int i = 0; i < 10; ++i) 
{    
	pool.run_detached([i] {         
		std::cout << "Задача " << i << " в пуле потоков" << std::endl;     
	}); 
}  
pool.shutdown();
```

## Parfor
`parfor` — это асинхронная параллельная версия цикла `for`, предназначенная для запуска задач на пуле потоков (`pot::executor`).  
Она автоматически делит диапазон итераций на **чанки** и выполняет их в нескольких потоках.

Поддерживается выполнение как синхронных, так и корутинных функций.

### Сигнатура
```cpp
template <int64_t static_chunk_size = -1, typename IndexType, typename FuncType = void(IndexType)> requires std::invocable<FuncType &, IndexType>
pot::coroutines::lazy_task<void>
parfor(pot::executor &executor, IndexType from, IndexType to, FuncType&& func);
```
### Параметры

|Параметр|Тип|Описание|
|---|---|---|
|`static_chunk_size`|`int64_t` (по умолчанию `-1`)|Размер чанка (кол-во итераций на задачу). Если `< 0`, размер рассчитывается автоматически на основе количества потоков в `executor`.|
|`IndexType`|целочисленный тип|Тип счётчика цикла (например, `int`, `std::size_t`).|
|`FuncType`|вызываемый объект `void(IndexType)` или `task`/`lazy_task`|Функция, вызываемая для каждой итерации. Может быть синхронной или асинхронной.|
|`executor`|`pot::executor&`|Исполнитель (пул потоков), на котором будут выполняться задачи.|
|`from`|`IndexType`|Начальное значение индекса (включительно).|
|`to`|`IndexType`|Конечное значение индекса (исключительно).|
|`func`|вызываемый объект|Функция или лямбда, принимающая индекс и выполняющая работу.|

### Возвращаемое значение

`pot::coroutines::lazy_task<void>` — ленивый таск, который завершится, когда все параллельные задания будут выполнены.

### Принцип работы

1. **Разделение диапазона на чанки**  
    Если `static_chunk_size < 0`, он вычисляется как:
    
    `chunk_size = max(1, numIterations / executor.thread_count())`
    
    Это позволяет балансировать нагрузку между потоками.
    
2. **Создание задач**  
    Для каждого чанка создаётся задача (`task<void>`), которая обрабатывает свой поддиапазон индексов.
    
3. **Поддержка корутин**  
    Если `func` возвращает `task` или `lazy_task`, они будут корректно `co_await`-нуты внутри.
    
4. **Синхронное ожидание завершения**  
    Все задачи синхронно дожидаются выполнения через `.sync_wait()` перед выходом из `parfor`.
    

### Пример использования

#### Синхронная функция

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

#### Асинхронная функция (корутина)

```cpp
#include "pot/algorithms/parfor.h"
#include "pot/executors/thread_pool_executor.h"

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

## Async condition variable
`async_condition_variable` — это **асинхронная условная переменная** для корутин C++20.  
Она позволяет одной или нескольким корутинам приостановиться до тех пор, пока не будет вызван метод `set()`, после чего все ожидающие корутины будут возобновлены.

В отличие от стандартных `std::condition_variable`, этот класс **не блокирует поток**, а **приостанавливает выполнение корутины**, возвращая управление планировщику/исполнителю.

### Сигнатура
```cpp
namespace pot::coroutines
{
    class async_condition_variable
    {
    public:
        async_condition_variable(bool set = false) noexcept;

        async_condition_variable(const async_condition_variable&) = delete;
        async_condition_variable& operator=(const async_condition_variable&) = delete;
        async_condition_variable(async_condition_variable&&) = delete;
        async_condition_variable& operator=(async_condition_variable&&) = delete;

        auto operator co_await() noexcept;

        void set() noexcept;
        void stop() noexcept;
        bool is_set() const noexcept;
        void reset() noexcept;
    };
}
```
### Методы

|Метод|Описание|
|---|---|
|`async_condition_variable(bool set = false)`|Конструктор. Можно сразу установить начальное состояние (`true` — уже "сигнализировано").|
|`operator co_await()`|Позволяет напрямую ожидать объект через `co_await`. Возвращает awaiter.|
|`set()`|Устанавливает флаг и возобновляет все ожидающие корутины.|
|`stop()`|Сбрасывает флаг и очищает список ожидающих корутин без их возобновления.|
|`is_set()`|Проверяет, установлен ли флаг.|
|`reset()`|Сбрасывает флаг (`false`). Ожидающие корутины не удаляются.|

### Пример использования
```cpp
#include "pot/coroutines/async_condition_variable.h"
#include "pot/coroutines/task.h"

pot::coroutines::async_condition_variable cv;

pot::coroutines::task<void> waiter(int id) {
    co_await cv; // Ждём сигнала
    std::cout << "Корутин " << id << " возобновлён!" << std::endl;
}

pot::coroutines::task<void> example() {
    auto task1 = waiter(1);
	auto task2 = waiter(2);
	auto task3 = waiter(3);
	
    std::cout << "Отправляем сигнал через 1 секунду..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    cv.set(); // Возобновляем всех ожидающих
    co_return;
}
```

## When_all
Комбинатор ожиданий, который завершает своё выполнение, когда **все** переданные awaitable-объекты (корутины/таски) завершатся. Подходит для случаев, когда нужно дождаться группы задач перед продолжением.

### Сигнатуры
```cpp
// 1) Диапазон итераторов
template <typename Iterator> requires std::forward_iterator<Iterator>
pot::coroutines::task<void> when_all(Iterator begin, Iterator end);

// 2) Контейнер
template <template <class, class...> class Container, typename FuturePtr, typename... OtherTypes>
pot::coroutines::task<void> when_all(Container<FuturePtr, OtherTypes...>& futures);

// 3) Вариадик
template <typename... Futures>
pot::coroutines::task<void> when_all(Futures&&... futures);
```
### Параметры и требования

- `Iterator` — как минимум `std::forward_iterator` по коллекции awaitable-объектов (обычно `task<T>`/`lazy_task<T>`).
    
- Контейнерная перегрузка принимает любой стандартоподобный контейнер (например, `std::vector<task<void>>`).
    
- Вариадическая перегрузка принимает произвольное число awaitable-объектов.
    

### Возвращаемое значение

`pot::coroutines::task<void>` — завершается после успешного завершения всех переданных задач.

### Примеры использования
1) Вариадик
```cpp
using pot::coroutines::task;
using pot::coroutines::when_all;

task<void> a();
task<void> b();
task<void> c();

task<void> run_all() {
    // Предполагается, что a/b/c уже запустятся до await (например, если это не ленивые таски)
    co_await when_all(a(), b(), c());
}
```
2) Контейнер
```cpp
std::vector<pot::coroutines::task<void>> tasks;
tasks.push_back(do_work(1));
tasks.push_back(do_work(2));
tasks.push_back(do_work(3));

co_await pot::coroutines::when_all(tasks);
```
3) Диапазон
```cpp
auto first = tasks.begin();
auto last  = tasks.end();
co_await pot::coroutines::when_all(first, last);
```
## Elementwise_reduce 
Асинхронная поэлементная редукция над двумя массивами. Сначала к каждой паре элементов применяется бинарная операция (`elem_op(a[i], b[i])`), затем результаты сводятся редукцией (`reduce_op`) с начальным элементом `identity`. Есть удобные перегрузки для `pointer`/`std::span`/`std::vector`.
### Сигнатуры
```cpp
// 1) Указатели
template <typename T, typename R = T,
          typename ElemOp = std::plus<T>,
          typename ReduceOp = std::plus<R>>
pot::coroutines::lazy_task<R>
elementwise_reduce(pot::executor& exec,
                   const T* a, const T* b, std::size_t n,
                   ElemOp elem_op, ReduceOp reduce_op, R identity)
  requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<R>);

// 2) std::span
template <typename T, typename R = T,
          typename ElemOp = std::plus<T>,
          typename ReduceOp = std::plus<T>>
pot::coroutines::lazy_task<R>
elementwise_reduce(pot::executor& exec,
                   std::span<const T> a, std::span<const T> b,
                   ElemOp elem_op, ReduceOp reduce_op, R identity);

// 3) std::vector
template <typename T, typename R, typename ElemOp, typename ReduceOp>
pot::coroutines::lazy_task<R>
elementwise_reduce(pot::executor& exec,
                   const std::vector<T>& a, const std::vector<T>& b,
                   ElemOp elem_op, ReduceOp reduce_op, R identity);
```

### Параметры и требования

- `T` — тип входных элементов (арифметический).
    
- `R` — тип результата (арифметический, по умолчанию `T`).
    
- `ElemOp` — вызываемый `(T, T) -> R`, применяется поэлементно.
    
- `ReduceOp` — вызываемый `(R, R) -> R`, сводит результаты.
    
- `exec` — `pot::executor` для планирования задач (используется `parfor`).
    
- Для перегрузок `span`/`vector` размеры должны совпадать, иначе `std::invalid_argument`.
    

### Возвращаемое значение

`pot::coroutines::lazy_task<R>` — завершается значением редукции (при `n == 0` возвращается `identity`).

### Пример использования

**Скаларное произведение**
```cpp
auto dot = [&](pot::executor& exec,
               const std::vector<double>& a,
               const std::vector<double>& b)
    -> pot::coroutines::lazy_task<double>
{
    co_return co_await pot::algorithms::elementwise_reduce<double,double>(
        exec, a, b,
        std::multiplies<double>{},
        std::plus<double>{},
        0.0
    );
};

```

## Elementwise_reduce_simd
SIMD-вариант поэлементной редукции: обрабатывает несколько элементов за итерацию через `pot::simd::simd_forced<..., ST>`, затем доредуцирует хвост скалярно.
### Сигнатуры
```cpp
// 1) Указатели (SIMD)
template <typename T, typename R = T,
          pot::simd::SIMDType ST,
          typename SimdElemOp,   // (simd_forced<T, ST>, simd_forced<T, ST>) -> simd_forced<R, ST>
          typename ScalarElemOp, // (T, T) -> R
          typename ReduceOp>     // (R, R) -> R
pot::coroutines::lazy_task<R>
elementwise_reduce_simd(pot::executor& exec,
                        const T* a, const T* b, std::size_t n,
                        SimdElemOp simd_elem_op,
                        ScalarElemOp scalar_elem_op,
                        ReduceOp reduce_op,
                        R identity)
  requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<R>);

// 2) std::span (SIMD)
template <typename T, typename R, pot::simd::SIMDType ST,
          typename SimdElemOp, typename ScalarElemOp, typename ReduceOp>
pot::coroutines::lazy_task<R>
elementwise_reduce_simd(pot::executor& exec,
                        std::span<const T> a, std::span<const T> b,
                        SimdElemOp simd_elem_op,
                        ScalarElemOp scalar_elem_op,
                        ReduceOp reduce_op,
                        R identity);

// 3) std::vector (SIMD)
template <typename T, typename R, pot::simd::SIMDType ST,
          typename SimdElemOp, typename ScalarElemOp, typename ReduceOp>
pot::coroutines::lazy_task<R>
elementwise_reduce_simd(pot::executor& exec,
                        const std::vector<T>& a, const std::vector<T>& b,
                        SimdElemOp simd_elem_op,
                        ScalarElemOp scalar_elem_op,
                        ReduceOp reduce_op,
                        R identity);
```
### Параметры и требования

- `ST` — конкретный векторный тип `pot::simd::SIMDType` (например, SSE/AVX и т.п.).
    
- `simd_elem_op` — операция на SIMD-регистры (возвращает SIMD-аккумулятор).
    
- `scalar_elem_op` — операция для хвостовых скалярных элементов.
    
- Остальные требования аналогичны скалярной версии.
    

### Возвращаемое значение

`pot::coroutines::lazy_task<R>` — результат редукции с использованием SIMD и параллельной обработки блоков.

### Пример использования
**L1-норма**
```cpp
template <typename T, pot::simd::SIMDType ST>
pot::coroutines::lazy_task<T>
l1_simd(pot::executor& exec, std::span<const T> a, std::span<const T> b)
{
    auto simd_abs_diff = [](auto va, auto vb){
        auto vd = va - vb;     // simd_forced<T, ST>
        return vd.abs();
    };
    auto scalar_abs_diff = [](T x, T y){ return std::abs(x - y); };

    co_return co_await pot::algorithms::elementwise_reduce_simd<T, T, ST>(
        exec, a, b, simd_abs_diff, scalar_abs_diff, std::plus<T>{}, T{0});
}
```

## Dot / Dot_simd
Асинхронное скалярное произведение двух массивов. Есть обычная и SIMD-версия; обе возвращают ленивую корутину с результатом.
### Сигнатуры
```cpp
// SIMD: std::span
template <typename T, pot::simd::SIMDType ST>
pot::coroutines::lazy_task<T>
dot_simd(pot::executor& exec, std::span<const T> a, std::span<const T> b)
  requires(std::is_arithmetic_v<T>);

// SIMD: std::vector
template <typename T, pot::simd::SIMDType ST>
pot::coroutines::lazy_task<T>
dot_simd(pot::executor& exec, const std::vector<T>& a, const std::vector<T>& b)
  requires(std::is_arithmetic_v<T>);

// Без SIMD: std::span
template <typename T>
pot::coroutines::lazy_task<T>
dot(pot::executor& exec, std::span<const T> a, std::span<const T> b)
  requires(std::is_arithmetic_v<T>);

// Без SIMD: std::vector
template <typename T>
pot::coroutines::lazy_task<T>
dot(pot::executor& exec, const std::vector<T>& a, const std::vector<T>& b)
  requires(std::is_arithmetic_v<T>);
```
### Параметры и требования

- `T` — арифметический тип элементов.
    
- `ST` — целевой SIMD-тип (`pot::simd::SIMDType`) для `dot_simd`.
    
- `exec` — `pot::executor` для распараллеливания.
    
- `a`, `b` — входные последовательности одинаковой длины (иначе `std::invalid_argument`).
    

### Возвращаемое значение

`pot::coroutines::lazy_task<T>` — завершается значением скалярного произведения.

### Примеры использования

1. Обычная версия (vector)
```cpp
std::vector<double> a = /* ... */, b = /* ... */;
auto res = co_await pot::algorithms::dot(exec, a, b);
```
2. SIMD-версия (span)
```cpp
pot::coroutines::lazy_task<float> run_simd(pot::executor& exec,
                                           std::span<const float> a,
                                           std::span<const float> b) {
    co_return co_await pot::algorithms::dot_simd<float, AVX>(exec, a, b);
}
```

## Parsections
Запускает несколько независимых секций параллельно на заданном исполнителе. Каждая секция — это вызываемый объект (`void()` или корутина, возвращающая `task<void>` / `lazy_task<void>`). Завершается, когда **все** секции отработают.

### Сигнатуры

```cpp
template<typename... Funcs> requires (std::is_invocable_v<Funcs> && ...) 
pot::coroutines::lazy_task<void> parsections(pot::executor& executor, Funcs&&... funcs);
```

### Параметры и требования

- `executor` — экземпляр `pot::executor`, на котором будут запущены все секции.
    
- `funcs...` — один или несколько вызываемых объектов:
    
    - синхронные `void()` функции/лямбда-функции;
        
    - корутины, возвращающие `pot::coroutines::task<void>` или `pot::coroutines::lazy_task<void>`.
        
- Требования:
    
    - как минимум один аргумент (`static_assert(sizeof...(Funcs) > 0)`).
        
    - каждый `Func` должен быть вызываем без аргументов (`std::is_invocable_v`).
        

### Возвращаемое значение

`pot::coroutines::lazy_task<void>` — завершается после завершения **всех** секций.

### Пример использования
```cpp
pot::coroutines::task<void> coroA();
pot::coroutines::lazy_task<void> coroB();

co_await pot::algorithms::parsections(exec,
    []                                       { prepare();                   },
    []() -> pot::coroutines::task<void>      { co_await coroA(); co_return; },
    []() -> pot::coroutines::lazy_task<void> { co_await coroB(); co_return; }
);
```
## Resume_on
Функция, возвращающая awaitable, которая возобновляет выполнение текущей корутины на заданном `executor`.

### Сигнатуры

```cpp
namespace pot::coroutines {  
	// Возвращает awaitable-объект; при await — планирует продолжение на executor 
	auto resume_on(pot::executor& executor) noexcept;  
}
```

### Поведение

- `co_await resume_on(exec)` всегда откладывает продолжение и передаёт `std::coroutine_handle<>` в `executor.run_detached(...)`.
    
- `await_ready()` всегда `false` — продолжение **всегда** переназначается на переданный `executor`.
    
- Без возвращаемого значения; ошибки (если есть) определяются реализацией `executor`.
    

### Параметры и требования

- `executor` — экземпляр `pot::executor`, способный принять `std::coroutine_handle<>` через `run_detached(handle)` и возобновить его на собственном планировщике.  

### Примеры использования
```cpp
using pot::coroutines::resume_on;

pot::coroutines::task<void> do_work(pot::executor& cpu1, pot::executor& cpu2) 
{
    co_await resume_on(cpu1);    // продолжить на CPU1
    co_await heavy_compute();    // тяжёлая работа на CPU1
    co_await resume_on(cpu2);    // продолжить на CPU2
    update();                    // update() на CPU2
    co_return;
}
```
