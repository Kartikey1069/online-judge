#include "testrunner/testrunner.hpp"

#include "common/execution_limits.hpp"

TestRunner::TestRunner(const IProcessRunner& runner,const IJudge& judge)
:runner (runner),judge(judge)
{}
        

TestRunnerResult  TestRunner::run(const std::string& executable_path,const TestSuite& testsuite,const ExecutionConfig& config){
    TestRunnerResult  runner_result;
    for(std::size_t i=0; i<testsuite.size();++i){
        const TestCase& test_case = testsuite.getTestCase(i);

        ExecutionResult execution_result = runner.run(executable_path,{},test_case.input,config);

        JudgeResult judge_result = judge.evaluate(execution_result,test_case.expected_output);

        if(judge_result.verdict != Verdict::Accepted){
            runner_result.failed_test_index = i;
            runner_result.failed_execution_result = execution_result;
            runner_result.failed_judge_result = judge_result;
            return runner_result;
        }
    }
    return  runner_result; 
}