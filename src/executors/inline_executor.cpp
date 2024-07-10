#include "pot/executors/inline_executor.h"

void pot::executors::inline_executor::derived_execute(const std::function<void()> func)
{
    func();
}
