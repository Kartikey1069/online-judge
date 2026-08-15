#pragma once 


#include <gmock/gmock.h>
#include "judge/i_judge.hpp"

class MockJudge: public IJudge{
    public:
        MOCK_METHOD(
            JudgeResult,
            evaluate,
            (
                const ExecutionResult& execution,
                const std::string& expected_output
            ),(const,override)
        );
};