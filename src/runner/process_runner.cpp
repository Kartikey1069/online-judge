#include "runner/process_runner.hpp"

#include <unistd.h>      // pipe, fork, write, _exit
#include <sys/wait.h>    // waitpid
#include <cstring>        // strlen
#include <iostream>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <poll.h>
#include <cstddef>
#include <cerrno>

constexpr std::size_t IO_BUFFER_SIZE = 4096;

ExecutionResult ProcessRunner::run(const std::string& executable_path,const std::vector<std::string>& args,const std::string& input){
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];
    ExecutionResult result{};
    result.exit_code=-1;
    if(pipe(stdin_pipe) == -1){
        return result;
    }
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
        close(stdin_pipe[1]);
        if(dup2(stdin_pipe[0],STDIN_FILENO) == -1){
            perror("dup2 for stdin failed");
            _exit(1);
        }
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
        close(stdin_pipe[0]);
        
      
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
    close(stdin_pipe[0]);
    

    pollfd fds[3];
    fds[0]={stdin_pipe[1],POLLOUT,0};
    fds[1]={stdout_pipe[0],POLLIN,0};
    fds[2]={stderr_pipe[0],POLLIN,0};
    char buffer_out[IO_BUFFER_SIZE];
    char buffer_err[IO_BUFFER_SIZE];
    bool stdin_open = true;
    bool stdout_open = true; 
    bool stderr_open = true;
    bool child_exited = false; 
    std::string stdout_output="";
    std::string stderr_output="";
    size_t input_offset = 0;
    int status=0;


    if(input.size() == 0){
            stdin_open=false;
            close(stdin_pipe[1]);
            fds[0].fd = -1;
    }

    std::cout<<"polling starting\n";
    int ready;
    while(!child_exited || stdin_open || stdout_open || stderr_open){
        
        std::cout << "before poll\n";

       if(stdin_open || stdout_open || stderr_open){
            do {
                ready = poll(fds, 3, -1);
            } while (ready == -1 && errno == EINTR);

            

            std::cout << "after poll\n";
            if(ready == -1){
                return result;
            }
            std::cout<<"polling done\n";
            if(stdin_open){
                if(fds[0].revents & POLLOUT){
                    std::size_t remaining = input.size()-input_offset;
                    std::size_t chunk_size = std::min(remaining,IO_BUFFER_SIZE);
                    ssize_t bytes_written; 
                    do{
                        bytes_written = write(stdin_pipe[1],input.data()+input_offset,chunk_size);
                    } while(bytes_written == -1  && errno == EINTR);
                    std::cout<<"writecompleted\n";
                    if(bytes_written == -1 ){
                        return result; 
                    }
                    input_offset+=bytes_written;
                    if(input.size() == input_offset){
                        stdin_open=false;
                        std::cout << "ALL INPUT WRITTEN — CLOSING STDIN PIPE\n";
                        close(stdin_pipe[1]);
                        std::cout<<"Closing pipe\n";
                        fds[0].fd = -1;
                    }
                }
            }
            
            if(stdout_open){
                if(fds[1].revents & (POLLIN | POLLHUP)){
                    ssize_t bytes_out;


                    do {
                        bytes_out = read(
                            stdout_pipe[0],
                            buffer_out,
                            sizeof(buffer_out)
                        );
                    } while (bytes_out == -1 && errno == EINTR);


                    if(bytes_out == -1){
                        perror("Bytes not found");
                        return result;
                    }
                    if(bytes_out == 0){
                        stdout_open = false;
                        std::cout << "ALL output read — CLOSING STDout PIPE\n";
                        close(stdout_pipe[0]);
                        std::cout<<"Closing pipe\n";
                        fds[1].fd = -1 ;
                        
                    }
                    else{
                        stdout_output+=std::string(buffer_out,bytes_out);
                    }
                }  
            }
            if(stderr_open){
                if(fds[2].revents & (POLLIN | POLLHUP)){
                    

                    ssize_t bytes_err;


                    do {
                        bytes_err = read(
                            stderr_pipe[0],
                            buffer_err,
                            sizeof(buffer_err)
                        );
                    } while (bytes_err == -1 && errno == EINTR);


                    if(bytes_err == -1){
                        perror("Bytes not found");
                        return result;
                    }
                    if(bytes_err == 0){
                        stderr_open = false;
                        std::cout << "ALL output read — CLOSING STDerr PIPE\n";
                        close(stderr_pipe[0]);
                        std::cout<<"Closing pipe\n";
                        fds[2].fd = -1;
                    }
                    else{
                        stderr_output+=std::string(buffer_err,bytes_err);
                    }

                }

            }
        }
        std::cout << "before waitpid\n";

        pid_t wait_result;

        do {
            wait_result = waitpid(pid, &status, WNOHANG);
        } while (wait_result == -1 && errno == EINTR);

        std::cout << "after waitpid: " << wait_result
                << " errno=" << errno << '\n';
        if(wait_result == -1){
            return result ;
        }
        if(wait_result == pid){
            std::cout<<"child finished\n";
            child_exited = true;
        }
    }    
    
    std::cout<<"polling completed\n";
    if(WIFEXITED(status)){
        result.exit_code=WEXITSTATUS(status);
    }

    result.stdout_output=stdout_output;
    result.stderr_output=stderr_output;
    std::cout<<"returning\n";
    return  result;
}