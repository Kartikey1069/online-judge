#include <iostream>
#include "submission/submission_service.hpp"
#include "compiler/compiler.hpp"

int main(){
    std::cout<<"Online Judge started.\n";

    SubmissionService service;
    // SubmissionResult result=service.evaluate("../tests/solution.cpp","5","10 20 30 40\n");
    std::cout<<"Judge Terminated.\n";    
    return 0;
}