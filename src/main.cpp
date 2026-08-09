#include <iostream>
#include "submission/submission_service.hpp"
#include "compiler/compiler.hpp"

int main(){
    std::cout<<"Online Judge started.\n";

    SubmissionService service;
    SubmissionResult result=service.evaluate("../tests/solution.cpp","made the first engine \n");
    
    
    
    return 0;
}