#include <gtest/gtest.h>
#include "runner/process_runner.hpp"
#include "support/fixture.hpp"
#include <iostream>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include "common/execution_config.hpp"
#include <chrono>
namespace {

bool canUseCgroupKill() {
    const std::string path = "/sys/fs/cgroup/online-judge/cgroup.kill";
    const int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);

    if (fd == -1) {
        return false;
    }

    constexpr char value = '1';
    const ssize_t written = write(fd, &value, sizeof(value));
    const int saved_errno = errno;
    close(fd);

    if (written == -1) {
        errno = saved_errno;
        return false;
    }

    return true;
}

const ExecutionLimits test_limits{
    .cpu_limit = std::chrono::seconds(1),
    .wall_limit = std::chrono::seconds(1),
    .memory_limit= std::size_t(1ULL * 1024 * 1024 * 1024)
};

}

const ExecutionConfig config{
    .limit = test_limits
};

TEST(ProcessRunner, RunsSuccessfulProcess) {

    ProcessRunner runner;

    ExecutionResult result = runner.run(getFixturePath("exit_zero"),{},"",config);

    EXPECT_EQ(result.exit_code,0);
    EXPECT_EQ(result.state, ExecutionState::Finished);
    EXPECT_EQ(result.status, ExecutionStatus::Completed);
}


TEST(ProcessRunner, CapturesStdout) {

    ProcessRunner runner;

    ExecutionResult result = runner.run(getFixturePath("print_stdout"),{},"",config);

    EXPECT_EQ(result.exit_code,0);

    EXPECT_EQ(result.stdout_output,"hello\n");
}


TEST(ProcessRunner, CapturesStderr) {
    ProcessRunner runner;
    ExecutionResult result = runner.run(getFixturePath("print_stderr"), {},"",config);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stderr_output, "error\n");
    EXPECT_EQ(result.stdout_output, "");
}


TEST(ProcessRunner, PassesInputToProcess) {
    ProcessRunner runner;
    ExecutionResult result = runner.run(getFixturePath("echo_stdin"), {},"hello\n",config);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, "hello\n");
}

TEST(ProcessRunner, ReturnsNonZeroExitCode) {
    ProcessRunner runner;


    ExecutionResult result =
        runner.run(getFixturePath("exit_nonzero"), {}, "",config);

    EXPECT_EQ(result.exit_code, 42);
}



TEST(ProcessRunner, HandlesInvalidExecutable) {
    ProcessRunner runner;

    ExecutionResult result =
        runner.run("/nonexistent/program", {}, "",config);

    EXPECT_EQ(result.exit_code, 1);
    EXPECT_EQ(result.status, ExecutionStatus::SandboxFailure);
}

TEST(ProcessRunner, CapturesLargeStdout) {
    ProcessRunner runner;

    std::string expected(10000, 'x');

    ExecutionResult result =
        runner.run(
            getFixturePath("large_output"),
            {},
            "",
            config
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
            input,
            config
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
            "",
            config
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
            "",
            config
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
            "",
            config
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
            "",
            config
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
    "",
    config
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
            "",
            config
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
            "",
            config
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
            "",
            config
        );

    std::string expected_stdout(10000, 'o');
    std::string expected_stderr(10000, 'e');

    EXPECT_EQ(result.exit_code, 0);
    EXPECT_EQ(result.stdout_output, expected_stdout);
    EXPECT_EQ(result.stderr_output, expected_stderr);
}


TEST(ProcessRunner, GeneratesUniqueExecutionIds) {
    const std::uint64_t first = ProcessRunner::generateExecutionId();
    const std::uint64_t second = ProcessRunner::generateExecutionId();

    EXPECT_NE(first, 0ULL);
    EXPECT_NE(second, 0ULL);
    EXPECT_NE(first, second);
}

TEST(ProcessRunner, ReportsSignalTermination) {
    if (!canUseCgroupKill()) {
        GTEST_SKIP() << "cgroup.kill is unavailable in this environment; privileged signal termination is host-only.";
    }

    ProcessRunner runner;

    // Use a very short wall limit so the supervisor externally kills the process.
    // NOTE: On WSL2, raise(SIGKILL/SIGTERM) from PID 1 inside a user-namespace
    // PID namespace is converted by the kernel to exit_group(0), losing signal
    // information. Reliable signal detection requires external termination
    // (supervisor-originated kill via cgroup.kill), which is what this test exercises.
    const ExecutionConfig short_limit{
        .limit = ExecutionLimits{
            .cpu_limit   = std::chrono::seconds(1),
            .wall_limit  = std::chrono::milliseconds(100),
            .memory_limit = std::size_t(1ULL * 1024 * 1024 * 1024)
        }
    };

    ExecutionResult result =
        runner.run(
            getFixturePath("terminate_by_signal"),
            {},
            "",
            short_limit
        );

    EXPECT_TRUE(result.terminated_by_signal);
    // External kill via cgroup.kill uses SIGKILL
    EXPECT_EQ(result.signal_number, SIGKILL);
}