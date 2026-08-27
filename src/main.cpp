#include <iostream>
#include "submission/submission_service.hpp"
#include "compiler/compiler.hpp"

int main(){
    std::cout<<"Online Judge started.\n";

    SubmissionService service;
    TestSuite testsuite;
    TestCase a1{
        "0",
        "0\n"
    };
    TestCase a2{
        "1",
        "1\n"
    };
    testsuite.addTestCase(a1);
    testsuite.addTestCase(a2);
    const ExecutionLimits test_limits{
    .cpu_limit = std::chrono::seconds(1),
    .wall_limit = std::chrono::seconds(8),
    .memory_limit= std::size_t(1ULL * 1024 * 1024 * 1024)
    };

    ExecutionConfig config;
    config.limit = test_limits;

    SubmissionResult result = service.evaluate("../tests/solution.cpp",testsuite,config);
    std::cout<<"Judge Terminated.\n";    
    return 0;
}