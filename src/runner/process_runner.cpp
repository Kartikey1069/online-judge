#include "runner/process_runner.hpp"
#include <unistd.h>     
#include <sys/wait.h>    
#include <cstring>        
#include <iostream>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <poll.h>
#include <cstddef>
#include <csignal>
#include <chrono>
#include <signal.h>
#include <sys/resource.h>
#include <cerrno>

constexpr std::size_t IO_BUFFER_SIZE = 4096;
constexpr auto PROCESS_CHECK_INTERVAL =
    std::chrono::milliseconds(10);

std::chrono::microseconds to_microseconds(const timeval& time ) {
                return std::chrono::seconds(time.tv_sec)
                    + std::chrono::microseconds(time.tv_usec);
}






ExecutionResult ProcessRunner::run(const std::string& executable_path,const std::vector<std::string>& args,const std::string& input,const ExecutionLimits& limits)const{
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];
    ExecutionResult result{};
    
    result.exit_code=1;
    if(pipe(stdin_pipe) == -1){
        return result;
    }
    if(pipe(stdout_pipe)==-1){
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return result;
    }
    if(pipe(stderr_pipe)==-1){
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);

        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return result;
    }

    auto start_time = std::chrono::steady_clock::now();

    const auto deadline = start_time + limits.wall_limit;

    pid_t pid=fork();
    
    if(pid==-1){
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);

        close(stdout_pipe[0]);
        close(stdout_pipe[1]);

        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

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


        //setting process group 
        if(setpgid(0, 0)==-1){
            perror("process group setup failed ");
            _exit(1);
        };

        struct rlimit time_limit{};

        time_limit.rlim_cur = std::chrono::duration_cast<std::chrono::seconds>(limits.cpu_limit).count();
        time_limit.rlim_max = time_limit.rlim_cur;

        if(setrlimit(RLIMIT_CPU,&time_limit) == -1){
            perror("time limit setting failed");
            _exit(1);
        }



        struct rlimit memory_limit;

        memory_limit.rlim_cur = limits.memory_limit;
        memory_limit.rlim_max = limits.memory_limit;

        if(setrlimit(RLIMIT_AS,&memory_limit)==-1){
            perror("memory limit setting failed");
            _exit(1);
        }
        if(execvp(argsv[0],argsv.data())==-1){
            perror("exec failed");
            _exit(1);
        }

    }
    setpgid(pid, pid);
    pid_t  process_group_id = pid;


    auto  terminate_process_group = [&]()-> bool {
        if(kill(-process_group_id,SIGKILL) == -1){
            if(errno == ESRCH){
                return true;
            }
            return  false;
        }
        return true;
    };


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
    pid_t wait_result;
    struct rusage usage{};
    int ready;




    auto reap_child=[&]()-> bool{
        if(child_exited){
            return true;
        }
        do{wait_result = wait4(pid,&status,0,&usage);
        
        }while(wait_result == -1 && errno == EINTR);

        if(wait_result != pid) {
            return false; 
        }
        child_exited = true; 
     
        return true;
    };


    auto finalize_resource_usage = [&]() {
        auto end_time = std::chrono::steady_clock::now();

        result.wall_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time-start_time
        );
        result.cpu_time = 
            to_microseconds(usage.ru_utime)+
            to_microseconds(usage.ru_stime);
        
        result.memory_usage_bytes = 
            static_cast<std::size_t>(usage.ru_maxrss)*1024;
    };




    auto close_pipes = [&]() {
        if (stdin_open) {
            close(stdin_pipe[1]);
           
            stdin_open = false;
            fds[0].fd = -1;
        }

        if (stdout_open) {
            close(stdout_pipe[0]);
            
            stdout_open = false;
            fds[1].fd = -1;
        }

        if (stderr_open) {
            close(stderr_pipe[0]);
            stderr_open = false;
            fds[2].fd = -1;
        }
    };


    auto finalize_status = [&]() {
        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status)) {
            result.terminated_by_signal = true;
            result.signal_number = WTERMSIG(status);
        }
    };



    auto cleanup_after_failure = [&]() {
        if (!child_exited) {
            terminate_process_group();

            if (reap_child()) {
                finalize_resource_usage();
                finalize_status();
            }
        }

        close_pipes();
    };
   

    

    if(input.size() == 0){
            stdin_open=false;
            close(stdin_pipe[1]);
           
            fds[0].fd = -1;
    }

    
   
    
        

    while (!child_exited || stdin_open ||stdout_open ||stderr_open) {

        auto now = std::chrono::steady_clock::now();

        if (!child_exited && now >= deadline) {

            // Check whether the child actually finished
            // before we kill it.
            do {
             
                wait_result = wait4(pid, &status,WNOHANG,&usage);
             
            } while (wait_result == -1 && errno == EINTR);

            if (wait_result == -1) {

                // FIX:
                // The child may still be alive.
                // Never simply close the pipes and return.
                // We must terminate/reap the submission first.
                cleanup_after_failure();

                return result;
            }

            // Child won the race.
            if (wait_result == pid) {

                child_exited = true;
              

                finalize_resource_usage();
                finalize_status();

                if (stdin_open) {
                    close(stdin_pipe[1]);

                    stdin_open = false;
                    fds[0].fd = -1;
                }

                // FIX:
                // The main submission has exited, but descendants may
                // still exist and may still hold stdout/stderr pipes.
                // Kill the remaining submission process group.
                terminate_process_group();
            }
            else if (wait_result == 0) {

                // Child is definitely still running.
                result.wall_time_limit_exceeded = true;
                

                if (!terminate_process_group()) {
                    close_pipes();
                    return result;
                }

                if (!reap_child()) {
                    close_pipes();
                    return result;
                }

                finalize_resource_usage();
                finalize_status();

                close_pipes();
                return result;
            }
        }


        int timeout;

        if (child_exited) {

            // FIX:
            // The child is already dead.
            //
            // Do NOT wait indefinitely here.
            //
            // We only want to observe events that are already pending
            // on stdout/stderr. If nothing is immediately available,
            // poll() returns 0 and we go around the loop again.
            
            timeout = 0;
        }
        else {

            now = std::chrono::steady_clock::now();

            auto remaining =std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            timeout = static_cast<int>(remaining.count());

           


            // Avoid passing a negative timeout to poll.
            if (timeout < 0) {
                timeout = 0;
            }
        }


        // FIX:
        // If every descriptor has already been closed and the child
        // has exited, there is absolutely nothing left to poll.
        //
        // The while-condition will terminate naturally, so don't
        // make an unnecessary poll() call.
        if (child_exited &&
            !stdin_open &&
            !stdout_open &&
            !stderr_open) {
            break;
        }

        if(stdin_open || stdout_open || stderr_open){
            do {
              
                ready = poll(fds, 3, timeout);
           
            } while (ready == -1 && errno == EINTR);

            if (ready == -1) {
                cleanup_after_failure();
                return result;
            }

            // poll() timed out.
            //
            // If child is still alive, this means we reached the current
            // deadline and should go back to the top and check it.
            //
            // If child has already exited, timeout == 0 simply means
            // there are currently no pending pipe events.
            


            if (stdin_open &&
                (fds[0].revents & POLLOUT)) {

                std::size_t remaining =
                    input.size() - input_offset;

                std::size_t chunk_size =
                    std::min(remaining, IO_BUFFER_SIZE);

                ssize_t bytes_written;

                do {
                    bytes_written = write(stdin_pipe[1],input.data() + input_offset,chunk_size);
                } while (bytes_written == -1 &&
                        errno == EINTR);

                if (bytes_written == -1) {
                    cleanup_after_failure();
                    return result;
                }

                input_offset += bytes_written;

                if (input_offset == input.size()) {

                    close(stdin_pipe[1]);
                  

                    stdin_open = false;
                    fds[0].fd = -1;
                }
            }


            if (stdout_open &&
                (fds[1].revents & (POLLIN | POLLHUP))) {

                ssize_t bytes_out;

                do {
                    bytes_out = read(stdout_pipe[0],buffer_out,sizeof(buffer_out));
                } while (bytes_out == -1 &&
                        errno == EINTR);

                if (bytes_out == -1) {
                    cleanup_after_failure();
                    return result;
                }

                if (bytes_out == 0) {

                    close(stdout_pipe[0]);
                 
                    stdout_open = false;
                    fds[1].fd = -1;
                }
                else {
                    stdout_output.append(
                        buffer_out,
                        bytes_out
                    );
                }
            }


            if (stderr_open &&
                (fds[2].revents & (POLLIN | POLLHUP))) {

                ssize_t bytes_err;

                do {
                    bytes_err = read(stderr_pipe[0],buffer_err,sizeof(buffer_err));
                } while (bytes_err == -1 &&
                        errno == EINTR);

                if (bytes_err == -1) {
                    cleanup_after_failure();
                    return result;
                }

                if (bytes_err == 0) {

                    close(stderr_pipe[0]);

                    stderr_open = false;
                    fds[2].fd = -1;
                }
                else {
                    stderr_output.append(
                        buffer_err,
                        bytes_err
                    );
                }
            }
        }

        if (!child_exited) {

            do {
                
             
                wait_result = wait4(pid, &status,WNOHANG,&usage);
              
            } while (wait_result == -1 &&
                    errno == EINTR);

            if (wait_result == -1) {

                // FIX:
                // Don't abandon a potentially running submission.
                cleanup_after_failure();

                return result;
            }

            if (wait_result == pid) {

                child_exited = true;
            
                finalize_resource_usage();
                finalize_status();

                if (stdin_open) {
                    close(stdin_pipe[1]);
                   
                    stdin_open = false;
                    fds[0].fd = -1;
                }

               
                terminate_process_group();
            }
        }
    }
   
    result.stdout_output=stdout_output;
    result.stderr_output=stderr_output;
    
    
    return  result;
}