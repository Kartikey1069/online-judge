#include <gtest/gtest.h>
#include <gmock/gmock.h>


#include "testrunner/testrunner.hpp"

#include "mocks/mock_process_runner.hpp"

#include "mocks/mock_judge.hpp"



using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Return;

TEST(TestRunner,AllTestCasesAccepted){
    TestCase test1{
        "0\n",
        "0\n"
    };
    TestCase test2{
        "1\n",
        "1\n"
    };


    MockProcessRunner mock_runner;
    MockJudge mock_judge;
    TestSuite testsuite; 



    testsuite.addTestCase(test1);
    testsuite.addTestCase(test2);

    ExecutionResult execution0;
    execution0.exit_code = 0;
    execution0.stdout_output = "0\n";
    ExecutionResult execution1;
    execution1.exit_code = 0;
    execution1.stdout_output = "1\n";


    JudgeResult judge0;
    judge0.verdict = Verdict::Accepted;

    JudgeResult judge1;
    judge1.verdict = Verdict::Accepted;


    EXPECT_CALL(mock_runner,run("solution",std::vector<std::string>{},"0\n")).WillOnce(testing::Return(execution0));
    
    EXPECT_CALL(
        mock_judge,
        evaluate(
            AllOf(
                Field(&ExecutionResult::exit_code, 0),
                Field(&ExecutionResult::stdout_output, "0\n")
            ),
            "0\n"
        )
    )
    .WillOnce(testing::Return(judge0));

    EXPECT_CALL(mock_runner,run("solution",std::vector<std::string>{},"1\n")).WillOnce(testing::Return(execution1));
    
    EXPECT_CALL(
        mock_judge,
        evaluate(
            AllOf(
                Field(&ExecutionResult::exit_code, 0),
                Field(&ExecutionResult::stdout_output, "1\n")
            ),
            "1\n"
        )
    )
    .WillOnce(testing::Return(judge0));
    

    TestRunner runner(mock_runner,mock_judge);
    TestRunnerResult result = runner.run("solution",testsuite);

    EXPECT_FALSE(result.failed_test_index.has_value());
    EXPECT_FALSE(result.failed_execution_result.has_value());
    EXPECT_FALSE(result.failed_judge_result.has_value());
    

}



TEST(TestRunner, StopsOnFirstFailure) {
    TestSuite testsuite;

    testsuite.addTestCase({"0\n", "0\n"});
    testsuite.addTestCase({"1\n", "1\n"});
    testsuite.addTestCase({"2\n", "2\n"});

    MockProcessRunner mock_runner;
    MockJudge mock_judge;

    ExecutionResult execution0;
    execution0.exit_code = 0;
    execution0.stdout_output = "0\n";

    ExecutionResult execution1;
    execution1.exit_code = 0;
    execution1.stdout_output = "wrong\n";

    JudgeResult judge0;
    judge0.verdict = Verdict::Accepted;

    JudgeResult judge1;
    judge1.verdict = Verdict::WrongAnswer;

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "0\n")
    ).WillOnce(Return(execution0));

    EXPECT_CALL(
        mock_judge,
        evaluate(
            AllOf(
                Field(&ExecutionResult::exit_code, 0),
                Field(&ExecutionResult::stdout_output, "0\n")
            ),
            "0\n"
        )
    ).WillOnce(Return(judge0));

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "1\n")
    ).WillOnce(Return(execution1));

    EXPECT_CALL(
        mock_judge,
        evaluate(
            AllOf(
                Field(&ExecutionResult::exit_code, 0),
                Field(&ExecutionResult::stdout_output, "wrong\n")
            ),
            "1\n"
        )
    ).WillOnce(Return(judge1));

    TestRunner runner(mock_runner, mock_judge);

    TestRunnerResult result =
        runner.run("solution", testsuite);

    EXPECT_TRUE(result.failed_test_index.has_value());
    EXPECT_EQ(result.failed_test_index.value(), 1);
}



TEST(TestRunner, PreservesFailedTestDetails) {
    TestSuite testsuite;

    testsuite.addTestCase({"0\n", "0\n"});
    testsuite.addTestCase({"1\n", "expected\n"});

    MockProcessRunner mock_runner;
    MockJudge mock_judge;

    ExecutionResult execution0;
    execution0.exit_code = 0;
    execution0.stdout_output = "0\n";

    ExecutionResult failed_execution;
    failed_execution.exit_code = 17;
    failed_execution.stdout_output = "unexpected\n";
    failed_execution.stderr_output = "error\n";

    JudgeResult judge0;
    judge0.verdict = Verdict::Accepted;

    JudgeResult failed_judge;
    failed_judge.verdict = Verdict::WrongAnswer;

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "0\n")
    ).WillOnce(Return(execution0));

    EXPECT_CALL(
        mock_judge,
        evaluate(_, "0\n")
    ).WillOnce(Return(judge0));

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "1\n")
    ).WillOnce(Return(failed_execution));

    EXPECT_CALL(
        mock_judge,
        evaluate(
            AllOf(
                Field(&ExecutionResult::exit_code, 17),
                Field(&ExecutionResult::stdout_output, "unexpected\n"),
                Field(&ExecutionResult::stderr_output, "error\n")
            ),
            "expected\n"
        )
    ).WillOnce(Return(failed_judge));

    TestRunner runner(mock_runner, mock_judge);

    TestRunnerResult result =
        runner.run("solution", testsuite);

    EXPECT_TRUE(result.failed_test_index.has_value());
    EXPECT_EQ(result.failed_test_index.value(), 1);

    EXPECT_TRUE(result.failed_execution_result.has_value());

    EXPECT_EQ(
        result.failed_execution_result->exit_code,
        17
    );

    EXPECT_EQ(
        result.failed_execution_result->stdout_output,
        "unexpected\n"
    );

    EXPECT_EQ(
        result.failed_execution_result->stderr_output,
        "error\n"
    );

    EXPECT_TRUE(result.failed_judge_result.has_value());

    EXPECT_EQ(
        result.failed_judge_result->verdict,
        Verdict::WrongAnswer
    );
}


TEST(TestRunner, DoesNotExecuteTestsAfterFailure) {
    TestSuite testsuite;

    testsuite.addTestCase({"0\n", "0\n"});
    testsuite.addTestCase({"1\n", "1\n"});
    testsuite.addTestCase({"2\n", "2\n"});

    MockProcessRunner mock_runner;
    MockJudge mock_judge;

    ExecutionResult execution0;
    execution0.exit_code = 0;

    ExecutionResult execution1;
    execution1.exit_code = 0;

    JudgeResult accepted;
    accepted.verdict = Verdict::Accepted;

    JudgeResult failed;
    failed.verdict = Verdict::WrongAnswer;

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "0\n")
    ).WillOnce(Return(execution0));

    EXPECT_CALL(
        mock_judge,
        evaluate(_, "0\n")
    ).WillOnce(Return(accepted));

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "1\n")
    ).WillOnce(Return(execution1));

    EXPECT_CALL(
        mock_judge,
        evaluate(_, "1\n")
    ).WillOnce(Return(failed));

  
    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "2\n")
    ).Times(0);

    EXPECT_CALL(
        mock_judge,
        evaluate(_, "2\n")
    ).Times(0);

    TestRunner runner(mock_runner, mock_judge);

    TestRunnerResult result =
        runner.run("solution", testsuite);

    EXPECT_TRUE(result.failed_test_index.has_value());
    EXPECT_EQ(result.failed_test_index.value(), 1);
}

TEST(TestRunner, PassesCorrectInputToProcessRunner) {
    TestSuite testsuite;

    testsuite.addTestCase({"apple\n", "ok\n"});
    testsuite.addTestCase({"banana\n", "ok\n"});
    testsuite.addTestCase({"cherry\n", "ok\n"});

    MockProcessRunner mock_runner;
    MockJudge mock_judge;

    ExecutionResult execution;
    execution.exit_code = 0;

    JudgeResult accepted;
    accepted.verdict = Verdict::Accepted;

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "apple\n")
    ).WillOnce(Return(execution));

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "banana\n")
    ).WillOnce(Return(execution));

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "cherry\n")
    ).WillOnce(Return(execution));

    EXPECT_CALL(
        mock_judge,
        evaluate(_, _)
    ).Times(3).WillRepeatedly(Return(accepted));

    TestRunner runner(mock_runner, mock_judge);

    runner.run("solution", testsuite);
}


TEST(TestRunner, PassesCorrectExpectedOutputToJudge) {
    TestSuite testsuite;

    testsuite.addTestCase({"A\n", "APPLE\n"});
    testsuite.addTestCase({"B\n", "BANANA\n"});
    testsuite.addTestCase({"C\n", "CHERRY\n"});

    MockProcessRunner mock_runner;
    MockJudge mock_judge;

    ExecutionResult execution;
    execution.exit_code = 0;

    JudgeResult accepted;
    accepted.verdict = Verdict::Accepted;

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, _)
    ).Times(3).WillRepeatedly(Return(execution));

    EXPECT_CALL(
        mock_judge,
        evaluate(_, "APPLE\n")
    ).WillOnce(Return(accepted));

    EXPECT_CALL(
        mock_judge,
        evaluate(_, "BANANA\n")
    ).WillOnce(Return(accepted));

    EXPECT_CALL(
        mock_judge,
        evaluate(_, "CHERRY\n")
    ).WillOnce(Return(accepted));

    TestRunner runner(mock_runner, mock_judge);

    runner.run("solution", testsuite);
}


TEST(TestRunner, HandlesEmptyTestSuite) {
    TestSuite testsuite;

    MockProcessRunner mock_runner;
    MockJudge mock_judge;

    EXPECT_CALL(
        mock_runner,
        run(_, _, _)
    ).Times(0);

    EXPECT_CALL(
        mock_judge,
        evaluate(_, _)
    ).Times(0);

    TestRunner runner(mock_runner, mock_judge);

    TestRunnerResult result =
        runner.run("solution", testsuite);

    EXPECT_FALSE(result.failed_test_index.has_value());
    EXPECT_FALSE(result.failed_execution_result.has_value());
    EXPECT_FALSE(result.failed_judge_result.has_value());
}


TEST(TestRunner, PreservesRuntimeErrorVerdict) {
    TestSuite testsuite;

    testsuite.addTestCase({"input\n", "expected\n"});

    MockProcessRunner mock_runner;
    MockJudge mock_judge;

    ExecutionResult execution;
    execution.exit_code = 1;
    execution.stderr_output = "runtime error\n";

    JudgeResult runtime_error;
    runtime_error.verdict = Verdict::RuntimeError;

    EXPECT_CALL(
        mock_runner,
        run("solution", std::vector<std::string>{}, "input\n")
    ).WillOnce(Return(execution));

    EXPECT_CALL(
        mock_judge,
        evaluate(_, "expected\n")
    ).WillOnce(Return(runtime_error));

    TestRunner runner(mock_runner, mock_judge);

    TestRunnerResult result =
        runner.run("solution", testsuite);

    EXPECT_TRUE(result.failed_judge_result.has_value());

    EXPECT_EQ(
        result.failed_judge_result->verdict,
        Verdict::RuntimeError
    );
}