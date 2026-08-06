#include <iostream>
#include "runner/process_runner.hpp"
#include "judge/judge.hpp"

int main(){
    ProcessRunner runner;
    Judge judge;

    ExecutionResult execute =
    runner.run("../tests/solution");

    std::string expectedoutput = "made the first engine \n";
    JudgeResult result = judge.evaluate(execute,expectedoutput);

    std::cout<<"Online Judge started.\n";
    std::cout<<result.verdict<<"\n";
    return 0;
}