#include "runner/process_runner.hpp"

ExecutionResult ProcessRunner::run(const std::string& executable_path){
    (void)executable_path;

    ExecutionResult result{};
    result.verdict=Verdict::InternalError;
    result.exit_code=-1;
    
    return  result;
}