#pragma once 

#include <string>

#include "judge/judge_result.hpp"

#include "common/execution_result.hpp"

#include "judge/i_judge.hpp"


class Judge: public IJudge
{
public:
    JudgeResult evaluate (
        const ExecutionResult& execution,
        const std::string& expected_output)const override;
};