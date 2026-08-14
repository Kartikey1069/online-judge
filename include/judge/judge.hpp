#pragma once 

#include <string>

#include "judge/judge_result.hpp"

#include "common/execution_result.hpp"


class Judge
{
public:
    JudgeResult evaluate (
        const ExecutionResult& execution,
        const std::string& expected_output)const;
};