#include "submission/submission_service.hpp"
#include <iostream>
#include "judge/judge.hpp"
#include "runner/process_runner.hpp"

SubmissionResult SubmissionService::evaluate(const std::string& solution_path,const std::string& expected_output){
    ProcessRunner runner;
    Judge judge;
    SubmissionResult result;
    ExecutionResult execution_result =
    runner.run("ls",{"-l","-a"});

    JudgeResult judge_result = judge.evaluate(execution_result,expected_output);

    result.execution_result = execution_result;
    result.judge_result = judge_result;
    std::cout<<result.execution_result.stdout_output<<"\n";
    return result;
}