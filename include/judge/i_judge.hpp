#pragma once

#include <string>

#include "judge/judge_result.hpp"
#include "common/execution_result.hpp"

class IJudge {
public:
    virtual ~IJudge() = default;

    virtual JudgeResult evaluate(
        const ExecutionResult& execution,
        const std::string& expected_output
    ) const = 0;
};