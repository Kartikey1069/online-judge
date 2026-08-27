#pragma once 

#include <string>
#include <vector>
#include "common/execution_result.hpp"
#include "runner/i_process_runner.hpp"

class ProcessRunner:public IProcessRunner
{
    public:
        ExecutionResult run(const std::string& executable_path,const std::vector<std::string>& args,const std::string& input,const ExecutionConfig& config)const override;
};