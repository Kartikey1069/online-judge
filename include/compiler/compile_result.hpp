#pragma once


#include <string>

struct CompileResult{

    int exit_code;
    std::string stderr_output;
    std::string executable_path;
};