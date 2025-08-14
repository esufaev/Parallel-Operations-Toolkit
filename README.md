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
