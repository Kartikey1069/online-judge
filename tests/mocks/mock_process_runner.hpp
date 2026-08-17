#pragma once 
#include <gmock/gmock.h>

#include "runner/i_process_runner.hpp"
#include "common/execution_limits.hpp"
class MockProcessRunner : public IProcessRunner {
    public:
        MOCK_METHOD(
            ExecutionResult,
            run,
            (
                const std::string& executable_path,
                const std::vector<std::string>& args,
                const std::string& input,
                const ExecutionLimits& limits
            ),
            (const,override)

        );

};