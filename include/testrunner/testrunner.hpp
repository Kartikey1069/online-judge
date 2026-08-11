#pragma once

#include "testcase/testsuite.hpp"
#include "runner/process_runner.hpp"
#include "judge/judge.hpp"
#include "testrunner/testrunner_result.hpp"

class TestRunner{

    private:
        const ProcessRunner& runner;
        const Judge& judge;
    public:
        TestRunner(const ProcessRunner& runner,const Judge& judge);
        TestRunnerResult run(const std::string& executable_path,const TestSuite& testsuite);
};