#pragma once 


#include <common/execution_result.hpp>

#include <judge/judge_result.hpp>

struct SubmissionResult{
    
    ExecutionResult execution_result;
    JudgeResult judge_result;
};