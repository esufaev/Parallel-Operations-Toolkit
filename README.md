# Parallel Operations Toolkit

Этот модуль предоставляет удобные обёртки над C++20 корутинами:

## Установка

Скоро

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
## Скалярное произведение
Высокопроизводительная реализация скалярного произведения с векторизацией (SIMD) и параллельной обработкой по блокам через `executor`.

### Сигнатуры
```cpp
// указатели + длина
template <typename T, pot::simd::SIMDType ST>
pot::coroutines::lazy_task<T>
dot_simd(pot::executor& exec, const T* a, const T* b, std::size_t n)
    requires(std::is_arithmetic_v<T>);

// vectors
template <typename T, pot::simd::SIMDType ST>
pot::coroutines::lazy_task<T>
dot_simd(pot::executor& exec, const std::vector<T>& a, const std::vector<T>& b)
    requires(std::is_arithmetic_v<T>);

// полу-указатели (a_begin/a_end + b_begin)
template <typename T, pot::simd::SIMDType ST>
pot::coroutines::lazy_task<T>
dot_simd(pot::executor& exec, const T* a_begin, const T* a_end, const T* b_begin)
    requires(std::is_arithmetic_v<T>);
```
### Параметры

- `T` — арифметический тип элементов.
    
- `ST` — стратегия SIMD из `pot::simd::SIMDType` (SSE/AVX/AVX512 ).
    
- `exec` — исполнитель (одно- или многопоточный).
    
- `a`, `b`, `n` — массивы и их размер; для перегрузок со `span`/`vector` размеры должны совпадать (иначе будет `std::invalid_argument`).

### Возвращаемое значение

`lazy_task<T>` — ленивая корутина, завершающаяся, когда всё посчитано. Результат — сумма типа `T`.

### Пример использования
```cpp
#include "pot/algorithms/dot_simd.h"
#include "pot/executors/thread_pool_executor_lfgq.h"

pot::coroutines::task<void> example() {
    std::vector<T> a = {/* ... */};
    std::vector<T> b = {/* ... */};

    pot::executors::thread_pool_executor_lfgq pool("dot-pool", 8);

    // как lazy_task<T>: можно co_await прямо здесь или позже
    T res = 
	    co_await pot::algorithms::dot_simd<float, pot::simd::SIMDType::AVX>(pool, a, b);

    std::cout << "dot(a,b) = " << res << "\n";
    co_return;
}
```