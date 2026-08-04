#include <iostream>

#include "runner/process_runner.hpp"

int main(){
    ProcessRunner runner;

    auto result = runner.run("./dummy");
    
    std::cout<<"Online Judge started.\n";
    
    return 0;
}