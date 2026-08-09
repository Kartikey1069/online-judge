#pragma once 

#include <optional>

#include <common/execution_result.hpp>

#include <compiler/compile_result.hpp>

#include <judge/judge_result.hpp>


struct SubmissionResult{
    CompileResult compile_result;
    std::optional<ExecutionResult> execution_result;
    std::optional<JudgeResult> judge_result;
    Verdict verdict;
};