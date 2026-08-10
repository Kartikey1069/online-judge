#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "runner/process_runner.hpp"

class ProcessRunnerTest : public ::testing::Test {
protected:
    ProcessRunner runner;
};

// 1. Normal input/output: Send standard input to 'cat' and check stdout echo
TEST_F(ProcessRunnerTest, NormalInputOutput) {
    std::string input = "Hello, ProcessRunner!\nLine 2\n";
    ExecutionResult result = runner.run("/bin/cat", {}, input);

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, input);
    EXPECT_TRUE(result.stderr_output.empty());
}

// 2. Empty input: Pass empty input string to 'cat'
TEST_F(ProcessRunnerTest, EmptyInput) {
    ExecutionResult result = runner.run("/bin/cat", {}, "");

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.stdout_output.empty());
    EXPECT_TRUE(result.stderr_output.empty());
}

// 3. Large stdout: Generate 1MB of stdout data to test buffer/polling loop
TEST_F(ProcessRunnerTest, LargeStdout) {
    std::size_t target_size = 1024 * 1024; // 1 MB
    std::string script = "import sys; sys.stdout.write('A' * " + std::to_string(target_size) + ")";

    ExecutionResult result = runner.run("/usr/bin/env", {"python3", "-c", script}, "");

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output.size(), target_size);
    EXPECT_EQ(result.stdout_output, std::string(target_size, 'A'));
    EXPECT_TRUE(result.stderr_output.empty());
}

// 4. Large stderr: Generate 1MB of stderr data
TEST_F(ProcessRunnerTest, LargeStderr) {
    std::size_t target_size = 1024 * 1024; // 1 MB
    std::string script = "import sys; sys.stderr.write('B' * " + std::to_string(target_size) + ")";

    ExecutionResult result = runner.run("/usr/bin/env", {"python3", "-c", script}, "");

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.stdout_output.empty());
    EXPECT_EQ(result.stderr_output.size(), target_size);
    EXPECT_EQ(result.stderr_output, std::string(target_size, 'B'));
}

// 5. Both stdout and stderr large simultaneously
// Verifies no deadlocks occur when pipe OS buffers (~64KB) fill up on both ends
TEST_F(ProcessRunnerTest, BothStdoutAndStderrLargeSimultaneously) {
    std::size_t target_size = 1024 * 1024; // 1 MB each
    std::string script = 
        "import sys\n"
        "sys.stdout.write('O' * " + std::to_string(target_size) + ")\n"
        "sys.stderr.write('E' * " + std::to_string(target_size) + ")\n";

    ExecutionResult result = runner.run("/usr/bin/env", {"python3", "-c", script}, "");

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output.size(), target_size);
    EXPECT_EQ(result.stderr_output.size(), target_size);
    EXPECT_EQ(result.stdout_output, std::string(target_size, 'O'));
    EXPECT_EQ(result.stderr_output, std::string(target_size, 'E'));
}

// 6. Program that waits for EOF: Ensures stdin pipe closing works properly
TEST_F(ProcessRunnerTest, ProgramWaitsForEOF) {
    std::string input = "Data block 1\nData block 2\n";
    // 'wc -l' counts lines until EOF on stdin
    ExecutionResult result = runner.run("/usr/bin/wc", {"-l"}, input);

    EXPECT_EQ(result.exit_code, 0);
    // wc outputs line count (2)
    EXPECT_NE(result.stdout_output.find("2"), std::string::npos);
    EXPECT_TRUE(result.stderr_output.empty());
}

// 7. Program that exits immediately: Handles fast process termination before I/O loop finishes
TEST_F(ProcessRunnerTest, ProgramExitsImmediately) {
    // 'true' exits with code 0 immediately without reading stdin
    ExecutionResult result = runner.run("/bin/true", {}, "Some unread input stream");

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.stdout_output.empty());
    EXPECT_TRUE(result.stderr_output.empty());
}

// 8. Program with non-zero exit code: Verifies exit status capture
TEST_F(ProcessRunnerTest, ProgramWithNonZeroExitCode) {
    // Execute a shell script exiting with status code 42
    ExecutionResult result = runner.run("/bin/sh", {"-c", "echo 'failed' >&2; exit 42"}, "");

    EXPECT_EQ(result.exit_code, 42);
    EXPECT_TRUE(result.stdout_output.empty());
    EXPECT_EQ(result.stderr_output, "failed\n");
}