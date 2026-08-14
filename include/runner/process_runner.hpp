#pragma once 

#include <string>
#include <vector>
#include "common/execution_result.hpp"

class ProcessRunner
{
    public:
        ExecutionResult run(const std::string& executable_path,const std::vector<std::string>& args,const std::string& input)const;
};