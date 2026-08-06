#include <iostream>
#include "submission/submission_service.hpp"

int main(){
    std::cout<<"Online Judge started.\n";
    
    SubmissionService service;
    SubmissionResult result=service.evaluate("../tests/solution","made the first engine \n");
    
    return 0;
}