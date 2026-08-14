#include <gtest/gtest.h>
#include "judge/judge.hpp"


TEST(Judge,AcceptsCorrectOutput) {
    ExecutionResult execution;
    execution.stdout_output="hello\n";
    execution.exit_code = 0;

    Judge judge;

    JudgeResult result = judge.evaluate(execution,"hello\n");

    EXPECT_EQ(result.verdict,Verdict::Accepted);

}


TEST(Judge,RejectsWrongOutput) { 
    ExecutionResult execution;
    execution.stdout_output="bye\n";
    execution.exit_code = 0;

    Judge judge;

    JudgeResult result = judge.evaluate(execution,"hello\n");

    EXPECT_EQ(result.verdict,Verdict::WrongAnswer);

}

TEST(Judge,RuntimeErrorWhenExecutionFails) { 
    ExecutionResult execution;
    execution.stdout_output="hello\n";
    execution.exit_code = 1;

    Judge judge;

    JudgeResult result = judge.evaluate(execution,"hello\n");

    EXPECT_EQ(result.verdict,Verdict::RuntimeError);

}


TEST(Judge, AcceptsEmptyOutput) {
    ExecutionResult execution;
    execution.exit_code = 0;
    execution.stdout_output = "";

    Judge judge;

    JudgeResult result =
        judge.evaluate(execution, "");

    EXPECT_EQ(result.verdict, Verdict::Accepted);
}