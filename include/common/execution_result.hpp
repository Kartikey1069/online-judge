#pragma once 

#include<string>
#include<chrono>
#include "common/verdict.hpp"


enum class ExecutionState {
    Created,
    Starting,
    Running,
    Finished,
    TimedOut,
    Signaled,
    Failed,
    SandboxFailed,
    RunnerFailed,
    Cancelled
};

enum class ExecutionStatus {
    Completed,
    TimedOut,
    Signaled,
    SandboxFailure,
    RunnerFailure
};
struct ExecutionResult
{
    ExecutionStatus status = ExecutionStatus::RunnerFailure;
    ExecutionState state = ExecutionState::Failed;
    int exit_code = 1;

    std::string stdout_output;

    std::string stderr_output;

    std::chrono::milliseconds wall_time{0};
    std::chrono::microseconds cpu_time{0};
    std::size_t memory_usage_bytes{0};

    bool terminated_by_signal=false;
    bool wall_time_limit_exceeded=false;
    int signal_number = 0;
};
