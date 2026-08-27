#pragma once

#include "testcase/testsuite.hpp"
#include "runner/i_process_runner.hpp"
#include "judge/i_judge.hpp"
#include "testrunner/testrunner_result.hpp"


class TestRunner{

    private:
        const IProcessRunner& runner;
        const IJudge& judge;
    public:
        TestRunner(const IProcessRunner& runner,const IJudge& judge);
        TestRunnerResult run(const std::string& executable_path,const TestSuite& testsuite,const ExecutionConfig& config);
};