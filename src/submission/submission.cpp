#include "submission/submission_service.hpp"
#include <iostream>
#include "judge/judge.hpp"
#include "runner/process_runner.hpp"
#include "compiler/compiler.hpp"


SubmissionResult SubmissionService::evaluate(const std::string& solution_path,const std::string& expected_output){
    ProcessRunner runner;
    Judge judge;
    Compiler compiler;
    SubmissionResult result;
    ExecutionResult execution_result;
    const std::string output_file = "../tests/solution";
    CompileResult compile_result = compiler.compile(solution_path,output_file);
    if(compile_result.exit_code == 0){
        execution_result = runner.run(output_file,{});
    }

    else{
        std::cout<<"compilation failed\n";
        std::cout<<compile_result.stderr_output;
        result.compile_result = compile_result;
        result.verdict = Verdict::CompilationError;
        return result;
    }
    

    JudgeResult judge_result = judge.evaluate(execution_result,expected_output);
    result.compile_result = compile_result;ok 
    result.execution_result = execution_result;
    result.judge_result = judge_result;
    std::cout<<result.execution_result.stdout_output<<"\n";
    result.verdict = result.judge_result.verdict;
    return result;
}