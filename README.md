# Parallel Operations Toolkit

Этот модуль предоставляет удобные обёртки над C++20 корутинами:

- **`task<T>`** — немедленно запускаемая корутина (eager).
    
- **`lazy_task<T>`** — лениво запускаемая корутина (lazy), стартует только при первом `await` или вызове `.get()`.

## Установка

Скоро

## Пример использования

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

