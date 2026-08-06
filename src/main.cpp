#include <iostream>
#include "runner/process_runner.hpp"

int main(){
    ProcessRunner runner;

    ExecutionResult result =
    runner.run("../tests/solution");
    
    std::cout<<"Online Judge started.\n";
    std::cout<<result.exit_code<<"\n";
    std::cout<<result.stdout_output<<"\n";
    return 0;
}