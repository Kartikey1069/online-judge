#include "runner/process_runner.hpp"

#include <unistd.h>      // pipe, fork, write, _exit
#include <sys/wait.h>    // waitpid
#include <cstring>        // strlen
#include <iostream>
#include <cstdio>
#include <vector>

ExecutionResult ProcessRunner::run(const std::string& executable_path,const std::vector<std::string>& args){
    int pipefd[2];
    ExecutionResult result{};
    result.exit_code=-1;

    if(pipe(pipefd)==-1){
        return result;
    }
    pid_t pid=fork();
    if(pid==-1){
        return result;
    }
    else if(pid==0){
        close(pipefd[0]);
        dup2(pipefd[1],STDOUT_FILENO);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1){
            perror("dup2 failed");
            _exit(1);
        }
        close(pipefd[1]);
        std::vector<char*>argsv;
        argsv.push_back(const_cast<char*>(executable_path.c_str()));
        for(auto it:args){
            argsv.push_back(const_cast<char*>(it.c_str()));
        }
        argsv.push_back(nullptr);
        if(execvp(argsv[0],argsv.data())==-1){
            perror("exec failed");
            _exit(1);
        }
        
    }
    close(pipefd[1]);
    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid failed");
        _exit(1);
    }
    if(WIFEXITED(status)){
        result.exit_code=WEXITSTATUS(status);
    }
    char buffer[4096];
    ssize_t bytes = read(pipefd[0],buffer,sizeof(buffer));
    close(pipefd[0]);
    if(bytes==-1){
        return result ;
    }
    result.stdout_output=std::string(buffer,bytes);
    return  result;
}