#pragma once 

#include <optional>



#include <compiler/compile_result.hpp>

#include <testrunner/testrunner.hpp>


struct SubmissionResult{
    CompileResult compile_result;
    Verdict verdict;
    TestRunnerResult  test_runner_result;
};