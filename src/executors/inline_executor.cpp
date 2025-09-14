#include "pot/executors/inline_executor.h"

void pot::executors::inline_executor::derived_execute(pot::utils::unique_function_once&& func)
{
    std::invoke(std::move(func));
}
