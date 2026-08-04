#pragma once 

#include<string>

#include "common/verdict.hpp"

struct ExecutionResult
{
    Verdict  verdict;
    std::string stdout_output;
    std::string stderr_output;
    int exit_code;
};
