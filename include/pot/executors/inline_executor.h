#pragma once

#include "pot/executors/executor.h"

namespace pot::executors
{
    class inline_executor;
}


class pot::executors::inline_executor final : public executor
{
public:
    explicit inline_executor(std::string name) : executor(std::move(name)) {}


    void shutdown() override {}


protected:
    void derived_execute(pot::utils::unique_function_once&& func) override;

};
