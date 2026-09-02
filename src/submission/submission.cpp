#include "submission/submission_service.hpp"
#include <iostream>
#include "judge/judge.hpp"
#include "runner/process_runner.hpp"
#include "compiler/compiler.hpp"


#include <filesystem>

SubmissionResult SubmissionService::evaluate(const std::string& solution_path,const TestSuite& testsuite,const ExecutionConfig& config){
    ProcessRunner runner;
    Judge judge;
    Compiler compiler;
    SubmissionResult result;
    TestRunner testrunner(runner,judge);
    TestRunnerResult runner_result;

    const std::filesystem::path source_path = solution_path;
    const std::filesystem::path output_file = source_path.parent_path() / "solution_exec";

    CompileResult compile_result = compiler.compile(solution_path, output_file.string());
    result.compile_result = compile_result;

    if(compile_result.exit_code == 0){
        runner_result = testrunner.run(output_file.string(), testsuite, config);
        result.test_runner_result = runner_result;
        if(runner_result.failed_test_index.has_value()){
           result.verdict = runner_result.failed_judge_result->verdict;
        }
        else{
            result.verdict = Verdict::Accepted;
        }
    }
    else{
        std::cout << "compilation failed\n";
        std::cout << compile_result.stderr_output;
        result.compile_result = compile_result;
        result.verdict = Verdict::CompilationError;
        return result;
    }

    return result;
}