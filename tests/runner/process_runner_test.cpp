#include <gtest/gtest.h>

#include "runner/process_runner.hpp"
#include "support/fixture.hpp"
#include <iostream>
#include <csignal>


TEST(ProcessRunner, RunsSuccessfulProcess) {

    ProcessRunner runner;

    ExecutionResult result = runner.run(getFixturePath("exit_zero"),{},"");

    EXPECT_EQ(result.exit_code,0);
}


TEST(ProcessRunner, CapturesStdout) {

    ProcessRunner runner;

    ExecutionResult result = runner.run(getFixturePath("print_stdout"),{},"");

    EXPECT_EQ(result.exit_code,0);

    EXPECT_EQ(result.stdout_output,"hello\n");
}


TEST(ProcessRunner, CapturesStderr) {
    ProcessRunner runner;
    ExecutionResult result = runner.run(getFixturePath("print_stderr"), {},"");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stderr_output, "error\n");
    EXPECT_EQ(result.stdout_output, "");
}


TEST(ProcessRunner, PassesInputToProcess) {
    ProcessRunner runner;
    ExecutionResult result = runner.run(getFixturePath("echo_stdin"), {},"hello\n");
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, "hello\n");
}

TEST(ProcessRunner, ReturnsNonZeroExitCode) {
    ProcessRunner runner;


    ExecutionResult result =
        runner.run(getFixturePath("exit_nonzero"), {}, "");

    EXPECT_EQ(result.exit_code, 42);
}



TEST(ProcessRunner, HandlesInvalidExecutable) {
    ProcessRunner runner;

    ExecutionResult result =
        runner.run("/nonexistent/program", {}, "");

    EXPECT_EQ(result.exit_code, 1);
}

TEST(ProcessRunner, CapturesLargeStdout) {
    ProcessRunner runner;

    std::string expected(10000, 'x');

    ExecutionResult result =
        runner.run(
            getFixturePath("large_output"),
            {},
            ""
        );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, expected);
}

TEST(ProcessRunner, PassesLargeStdin) {
    ProcessRunner runner;

    std::string input(10000, 'x');

    ExecutionResult result =
        runner.run(
            getFixturePath("echo_stdin"),
            {},
            input
        );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, input);
}

TEST(ProcessRunner, HandlesEmptyInput) {
    ProcessRunner runner;

    ExecutionResult result =
        runner.run(
            getFixturePath("echo_stdin"),
            {},
            ""
        );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, "");
}

TEST(ProcessRunner, HandlesEmptyStdout) {
    ProcessRunner runner;

    ExecutionResult result =
        runner.run(
            getFixturePath("exit_zero"),
            {},
            ""
        );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, "");
}


TEST(ProcessRunner, HandlesEmptyStderr) {
    ProcessRunner runner;

    ExecutionResult result =
        runner.run(
            getFixturePath("exit_zero"),
            {},
            ""
        );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stderr_output, "");
}

TEST(ProcessRunner, CapturesStdoutAndStderrSeparately) {
    ProcessRunner runner;

    ExecutionResult result =
        runner.run(
            getFixturePath("print_both"),
            {},
            ""
        );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, "out\n");
    EXPECT_EQ(result.stderr_output, "err\n");
}


TEST(ProcessRunner, PassesArgument) {
    ProcessRunner runner;

    ExecutionResult result = runner.run(
    getFixturePath("print_args"),
    {"hello", "world", "42"},
    ""
    );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, "hello\nworld\n42\n");
    
}

TEST(ProcessRunner, PreservesOutputWithoutTrailingNewline) {
    ProcessRunner runner;

    ExecutionResult result =
        runner.run(
            getFixturePath("print_no_newline"),
            {},
            ""
        );

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, "hello");
}

TEST(ProcessRunner, CapturesBinaryOutput) {
    ProcessRunner runner;

    ExecutionResult result =
        runner.run(
            getFixturePath("print_binary"),
            {},
            ""
        );

    std::string expected{'a', '\0', 'b'};

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output.size(), 3);
    EXPECT_EQ(result.stdout_output, expected);
}


TEST(ProcessRunner, CapturesLargeStdoutAndStderr) {
    ProcessRunner runner;

    ExecutionResult result =
        runner.run(
            getFixturePath("large_both"),
            {},
            ""
        );

    std::string expected_stdout(10000, 'o');
    std::string expected_stderr(10000, 'e');

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, expected_stdout);
    EXPECT_EQ(result.stderr_output, expected_stderr);
}


TEST(ProcessRunner, ReportsSignalTermination) {
    ProcessRunner runner;

    ExecutionResult result =
        runner.run(
            getFixturePath("terminate_by_signal"),
            {},
            ""
        );

    EXPECT_TRUE(result.terminated_by_signal);
    EXPECT_EQ(result.signal_number, SIGTERM);
}