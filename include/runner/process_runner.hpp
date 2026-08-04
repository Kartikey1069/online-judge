#pragma once 

#include <string>

#include "common/execution_result.hpp"

class ProcessRunner
{

    public:
        ExecutionResult run(const std::string& executable_path);
};