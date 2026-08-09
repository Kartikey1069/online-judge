#include "runner/process_runner.hpp"

#include <unistd.h>      // pipe, fork, write, _exit
#include <sys/wait.h>    // waitpid
#include <cstring>        // strlen
#include <iostream>
#include <cstdio>
#include <vector>

ExecutionResult ProcessRunner::run(const std::string& executable_path,const std::vector<std::string>& args){
    int stdout_pipe[2];
    int stderr_pipe[2];
    ExecutionResult result{};
    result.exit_code=-1;

    if(pipe(stdout_pipe)==-1){
        return result;
    }
    if(pipe(stderr_pipe)==-1){
        return result;
    }
    pid_t pid=fork();
    if(pid==-1){
        return result;
    }
    else if(pid==0){
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        if(dup2(stderr_pipe[1],STDERR_FILENO) == -1){
            perror("dup2 for stderr failed");
            _exit(1);
        }
        if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1){
            perror("dup2 for stdout failed");
            _exit(1);
        }

        close(stderr_pipe[1]);
        close(stdout_pipe[1]);
        
        std::vector<char*>argsv;
        argsv.push_back(const_cast<char*>(executable_path.c_str()));
        for(const auto& it:args){
            argsv.push_back(const_cast<char*>(it.c_str()));
        }
        argsv.push_back(nullptr);
        if(execvp(argsv[0],argsv.data())==-1){
            perror("exec failed");
            _exit(1);
        }
        
    }
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    int status;
    if (waitpid(pid, &status, 0) == -1){
       return result;
    }


    if(WIFEXITED(status)){
        result.exit_code=WEXITSTATUS(status);
    }


    char buffer_out[4096];
    ssize_t bytes_out = read(stdout_pipe[0],buffer_out,sizeof(buffer_out));
    if(bytes_out == -1){
        perror("Bytes not found");
        _exit(1);
    }


    char buffer_err[4096];
    ssize_t bytes_err = read(stderr_pipe[0],buffer_err,sizeof(buffer_err));
    if(bytes_err == -1){
        perror("Bytes not found");
        _exit(1);
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    
    result.stdout_output=std::string(buffer_out,bytes_out);
    result.stderr_output=std::string(buffer_err,bytes_err);
    
    return  result;
}