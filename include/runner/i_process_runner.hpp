#pragma once 

#include <string>
#include "common/execution_result.hpp"
#include <vector> 

class IProcessRunner{
    public:
        virtual ~IProcessRunner() = default;
        

        virtual  ExecutionResult run(
            const std::string& executable_path,
            const std::vector<std::string>& args,
            const std::string& input
        )const=0;
};