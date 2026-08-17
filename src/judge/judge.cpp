#include <iostream>

#include "judge/judge.hpp"
#include <signal.h>

JudgeResult Judge::evaluate(
    const ExecutionResult& execution,
    const std::string& expected_output)const {
        JudgeResult result;
        if(execution.terminated_by_signal){
            if(execution.wall_time_limit_exceeded){
                result.verdict = Verdict::TimeLimitExceeded;
            }
            else if(execution.signal_number == SIGXCPU){
                result.verdict = Verdict::TimeLimitExceeded;
            }
            else if(execution.signal_number == SIGSEGV){
                result.verdict = Verdict::RuntimeError;
            }
        }
        else{
            if(execution.exit_code != 0){
                result.verdict = Verdict::RuntimeError;
            }
            else if(execution.stdout_output != expected_output){
                result.verdict = Verdict::WrongAnswer;
            }
            else{
                result.verdict = Verdict::Accepted;
            }
        }
        
        return result;

}