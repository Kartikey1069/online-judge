#pragma once 

#include "common/verdict.hpp"

#include "common/execution_result.hpp"

#include "judge/judge_result.hpp"

#include <optional> 
#include <cstddef>
#include <string>

struct  TestRunnerResult{
    std::optional<std::size_t> failed_test_index;
    std::optional<ExecutionResult> failed_execution_result;
    std::optional<JudgeResult> failed_judge_result;
     //nullopt means all tests passed
};