#include <iostream>

#include "judge/judge.hpp"



JudgeResult Judge::evaluate(
    const ExecutionResult& execution,
    const std::string& expected_output)const {
        JudgeResult result;
        if(execution.exit_code != 0){
            result.verdict = Verdict::RuntimeError;
        }
        else if(execution.stdout_output != expected_output){
            result.verdict = Verdict::WrongAnswer;
        }
        else{
            result.verdict = Verdict::Accepted;
        }
        return result;

}