#pragma once 

#include<string>
#include<chrono>
#include "common/verdict.hpp"

struct ExecutionResult
{
    int exit_code = 1;

    std::string stdout_output;

    std::string stderr_output;

    std::chrono::milliseconds execution_time{0};

    std::size_t memory_used = 0;

    bool terminated_by_signal=false;

    int signal_number = 0;
};
