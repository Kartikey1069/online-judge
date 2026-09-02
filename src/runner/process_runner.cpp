#include "runner/process_runner.hpp"
#include "sandbox/sandbox.hpp"
#include "common/sandbox_protocol.hpp"
#include <unistd.h>     
#include <sys/wait.h>    
#include <cstring>        
#include <iostream>
#include <cstdio>
#include <sys/socket.h>
#include <vector>
#include <fcntl.h>
#include <algorithm>
#include <atomic>
#include <poll.h>
#include <cstddef>
#include <csignal>
#include <chrono>
#include <signal.h>
#include <csignal>
#include <sys/resource.h>
#include <cerrno>

constexpr std::size_t IO_BUFFER_SIZE = 4096;
constexpr auto PROCESS_CHECK_INTERVAL =
    std::chrono::milliseconds(10);

std::uint64_t ProcessRunner::generateExecutionId() {
    static std::atomic<std::uint64_t> counter{1};

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    const std::uint64_t pid_bits = static_cast<std::uint64_t>(getpid()) & 0xFFFFULL;

    const std::uint64_t id = (static_cast<std::uint64_t>(nanos) << 16) ^
                            (pid_bits << 8) ^
                            static_cast<std::uint64_t>(counter.fetch_add(1, std::memory_order_relaxed));

    return id == 0 ? 1ULL : id;
}

std::chrono::microseconds to_microseconds(const timeval& time ) {
                return std::chrono::seconds(time.tv_sec)
                    + std::chrono::microseconds(time.tv_usec);
}

class SigpipeGuard {
    public:
        SigpipeGuard() {
            struct sigaction ignore{};

            ignore.sa_handler = SIG_IGN;
            sigemptyset(&ignore.sa_mask);
            ignore.sa_flags = 0;

            if (sigaction(SIGPIPE, &ignore, &old_) == -1) {
                valid_ = false;
            }
        }

        ~SigpipeGuard() {
            if (valid_) {
                sigaction(SIGPIPE, &old_, nullptr);
            }
        }

        bool valid() const {
            return valid_;
        }

    private:
        struct sigaction old_{};
        bool valid_ = true;
};

ExecutionResult ProcessRunner::run(const std::string& executable_path,const std::vector<std::string>& args,const std::string& input,const ExecutionConfig& config)const{
    
    const std::uint64_t execution_id = ProcessRunner::generateExecutionId();
    ExecutionResult result{};
    result.state = ExecutionState::Created;
    
    result.exit_code=1;
    SigpipeGuard sigpipe_guard;
    if(!sigpipe_guard.valid()) {
        result.state = ExecutionState::RunnerFailed;
        result.status = ExecutionStatus::RunnerFailure;
        return  result;
    }

    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];

    int control_socket[2];

    

    
    
    if(socketpair(AF_UNIX,SOCK_STREAM | SOCK_CLOEXEC,0,control_socket) == -1){
        result.state = ExecutionState::RunnerFailed;
        result.status = ExecutionStatus::RunnerFailure;
        return result;
    }

    if(pipe(stdin_pipe) == -1){
        close(control_socket[0]);
        close(control_socket[1]);
        result.state = ExecutionState::RunnerFailed;
        result.status = ExecutionStatus::RunnerFailure;
        return result;
    }
    if(pipe(stdout_pipe)==-1){
        close(control_socket[0]);
        close(control_socket[1]);

        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        result.state = ExecutionState::RunnerFailed;
        result.status = ExecutionStatus::RunnerFailure;
        return result;
    }
    if(pipe(stderr_pipe)==-1){
        close(control_socket[0]);
        close(control_socket[1]);


        close(stdin_pipe[0]);
        close(stdin_pipe[1]);

        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        result.state = ExecutionState::RunnerFailed;
        result.status = ExecutionStatus::RunnerFailure;
        return result;
    }

    auto start_time = std::chrono::steady_clock::now();

    const auto deadline = start_time + config.limit.wall_limit;
    std::cerr << "ProcessRunner before fork: uid=" << getuid()
          << " gid=" << getgid() << '\n';

    pid_t supervisor_pid = fork();
    
    if(supervisor_pid==-1){
        close(control_socket[0]);
        close(control_socket[1]);

        close(stdin_pipe[0]);
        close(stdin_pipe[1]);

        close(stdout_pipe[0]);
        close(stdout_pipe[1]);

        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        result.state = ExecutionState::RunnerFailed;
        result.status = ExecutionStatus::RunnerFailure;
        return result;
    }
    else if(supervisor_pid==0){
        std::cerr << "Supervisor after fork: uid=" << getuid()
              << " gid=" << getgid() << '\n';
        
        close(control_socket[0]);
        
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        close(stdin_pipe[1]);


        //supervisor from here
        SandboxExecutionContext context {
            .stdin_fd = stdin_pipe[0],
            .stdout_fd = stdout_pipe[1],
            .stderr_fd = stderr_pipe[1],
            .control_fd = control_socket[1],
            .status_fd = -1
        };
        SandboxExecutionSpec spec {
            .executable_path = executable_path,
            .args = args,
            .cpu_limit = config.limit.cpu_limit,
            .memory_limit = config.limit.memory_limit
        };

        Sandbox sandbox(config.sandbox);
        
        SandboxSetupResult setup =sandbox.prepareEnvironment(control_socket[1],execution_id);
        
        if(!setup.success){
            perror("sanbox failed");
            _exit(EXIT_FAILURE);
        }
        
        SandboxProcess process = sandbox.createExecutionProcess(context,spec);
        

        if(process.host_pid <= 0) {
            if (!sandbox.cleanupCgroup()) {
                close(control_socket[1]);
                _exit(EXIT_FAILURE);
            }
            close(control_socket[1]);
            _exit(EXIT_FAILURE);
        }

        int status = 0;
        struct rusage usage{};
        bool submission_finished = false; 
        bool  startup_status_open = true;
        bool startup_failed= false;
        pid_t waited_pid = -1;
        int forwarded_child_status = 0;
        bool has_forwarded_status = false;
        
        const int control_fd = control_socket[1];
        
        pollfd poll_fds[2];

        poll_fds[0] = {control_fd,POLLIN,0};
        poll_fds[1] = {process.status_fd,POLLIN,0};


        auto cleanup_supervisor_failure = [&](bool termination_already_attempted) {
            if (!termination_already_attempted) {
                if (!sandbox.terminate()) {
                    // Cgroup termination failed.
                    // ProcessRunner remains the final containment authority.
                }
            }

            if (kill(process.host_pid, SIGKILL) == -1 &&
                errno != ESRCH) {
                // Cannot do anything stronger at this layer.
            }

            int submission_status = 0;
            struct rusage submission_usage{};

            pid_t waited;

            do {
                waited = wait4(
                    process.host_pid,
                    &submission_status,
                    0,
                    &submission_usage
                );
            } while (waited == -1 && errno == EINTR);

            if (startup_status_open) {
                close(process.status_fd);
                process.status_fd = -1;
                startup_status_open = false;
                poll_fds[1].fd = -1;
            }

            close(control_fd);

            _exit(EXIT_FAILURE);
        };
        bool termination_requested = false;
        const SandboxReadyPayload payload{
            .host_pid = process.host_pid,
            .namespace_pid = process.namespace_pid
        };
        
        const bool sent = send_message(control_socket[1],SandboxMessageType::Ready,&payload,sizeof(payload));

        if(!sent) {
            cleanup_supervisor_failure(termination_requested);
        }

        
        
        while(!submission_finished){
            int poll_result;
            do{

                poll_result = poll(poll_fds,2,10);

            } while(poll_result == -1 && errno == EINTR);
            
            if(poll_result == -1){
                cleanup_supervisor_failure(termination_requested);
            }

            if(poll_result > 0 &&  (poll_fds[0].revents & POLLIN)) {


                SandboxMessageHeader header {};
                if(!read_all(control_fd,&header,sizeof(header))) {
                    cleanup_supervisor_failure(termination_requested);
                }

                if(header.type == SandboxMessageType::Terminate) {
                    if(header.payload_size != 0) {
                        cleanup_supervisor_failure(termination_requested);
                    }

                    if(!termination_requested) {
                        if(!sandbox.terminate()) {
                            cleanup_supervisor_failure(true);
                        }
                        termination_requested = true;
                    }
                }
                else{
                    cleanup_supervisor_failure(termination_requested);
                }
            }    

            if (startup_status_open && (poll_fds[1].revents & (POLLIN | POLLHUP | POLLERR))) {

                SandboxSetupFailedPayload failure{};
                int forwarded_wait_status = 0;

                const StatusPipeReadResult read_result =
                    read_status_pipe(process.status_fd, failure, forwarded_wait_status);

                if (read_result == StatusPipeReadResult::WaitStatus) {
                    // Child A forwarded Child B's real wait_status.
                    // This is needed on WSL2 where SIGKILL on PID 1 in a
                    // user-namespace becomes exit_group(0), losing signal info.
                    // We record it now; the supervisor's own wait4 result will
                    // be overridden with this value when the child exits.
                    forwarded_child_status = forwarded_wait_status;
                    has_forwarded_status = true;
                    // Don't close the pipe yet — Child A will close it
                    // after writing, producing EOF which we handle below.
                }

                if (read_result == StatusPipeReadResult::Payload) {

                    close(process.status_fd);
                    process.status_fd = -1;
                    startup_status_open = false;
                    poll_fds[1].fd = -1;

                    do {
                        waited_pid =
                            wait4(process.host_pid, &status, 0, &usage);
                    } while (waited_pid == -1 && errno == EINTR);

                    if (waited_pid != process.host_pid) {
                         cleanup_supervisor_failure(termination_requested);
                    }
                    if (!sandbox.cleanupCgroup()) {
                        close(control_fd);
                        _exit(EXIT_FAILURE);
                    }

                    const bool sent = send_message(
                        control_fd,
                        SandboxMessageType::SetupFailed,
                        &failure,
                        sizeof(failure)
                    );

                    close(control_fd);

                    if (!sent) {
                        _exit(EXIT_FAILURE);
                    }

                    _exit(EXIT_FAILURE);
                }

                if (read_result == StatusPipeReadResult::Eof) {

                    close(process.status_fd);
                    process.status_fd = -1;

                    startup_status_open = false;
                    poll_fds[1].fd = -1;
                }

                if (read_result == StatusPipeReadResult::Error) {
                    cleanup_supervisor_failure(termination_requested);
                }
            }
            
            do{
                int raw_status = 0;
                (waited_pid = wait4(process.host_pid, &raw_status, WNOHANG,&usage));
                if (waited_pid == process.host_pid && !has_forwarded_status) {
                    status = raw_status;
                }
            } while(waited_pid == -1  && errno == EINTR);
            if(waited_pid == -1) {
                cleanup_supervisor_failure(termination_requested);
            }
            if(waited_pid == process.host_pid) {
                submission_finished = true;
                if (startup_status_open) {
                    close(process.status_fd);
                    process.status_fd = -1;
                    startup_status_open = false;
                    poll_fds[1].fd = -1;
                }
                if (!sandbox.cleanupCgroup()) {
                    close(control_fd);
                    _exit(EXIT_FAILURE);
                }
            }
        }
       

        if(waited_pid == -1) {
            const int error_code = errno;

            SandboxSetupFailedPayload payload{
                SandboxSetupStage::ProcessCreation,
                error_code
            };

            send_message(
                context.control_fd,
                SandboxMessageType::SetupFailed,
                &payload,
                sizeof(payload)
            );

            close(context.control_fd);
            _exit(EXIT_FAILURE);
        }

        SandboxTerminatedPayload terminated{
            .wait_status = has_forwarded_status ? forwarded_child_status : status,
            .usage = usage 
        };

        if(!send_message(context.control_fd,SandboxMessageType::Terminated,&terminated,sizeof(terminated))) {
            close(context.control_fd);
            _exit(EXIT_FAILURE);
        }
        
        close(context.control_fd);
        _exit(EXIT_SUCCESS);

    }
    close(control_socket[1]);
    auto execution_cgroup_path = [&]() {
        return std::string("/sys/fs/cgroup/online-judge/execution-")+std::to_string(execution_id);
    };
    auto is_execution_cgroup_empty = [&]() -> bool {
        const std::string path =
            execution_cgroup_path() + "/cgroup.events";

        const int fd = open(path.c_str(), O_RDONLY);

        if (fd == -1) {
            return false;
        }

        char buffer[256];
        ssize_t bytes_read;

        do {
            bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        } while (bytes_read == -1 && errno == EINTR);

        const int saved_errno = errno;
        close(fd);

        if (bytes_read == -1) {
            errno = saved_errno;
            return false;
        }

        buffer[bytes_read] = '\0';

        const char* populated = std::strstr(buffer, "populated ");

        if (populated == nullptr) {
            return false;
        }

        populated += std::strlen("populated ");

        return *populated == '0';
    };
    auto cleanup_execution_cgroup = [&]() -> bool {
        const std::string path = execution_cgroup_path();

        if (access(path.c_str(), F_OK) == -1) {
            return true;
        }

        if (rmdir(path.c_str()) == -1) {
            if (errno == EPERM || errno == EACCES || errno == ENOENT) {
                return true;
            }
            return false;
        }

        return true;
    };
    auto kill_execution_cgroup = [&]() -> bool {
        const std::string path =
            execution_cgroup_path() + "/cgroup.kill";

        const int fd = open(path.c_str(), O_WRONLY);

        if (fd == -1) {
            if (errno == EPERM || errno == EACCES || errno == ENOENT) {
                return true;
            }
            return false;
        }

        const char value = '1';

        ssize_t written;

        do {
            written = write(
                fd,
                &value,
                sizeof(value)
            );
        } while (
            written == -1 &&
            errno == EINTR
        );

        const int saved_errno = errno;

        close(fd);

        if (written == -1 && (saved_errno == EPERM || saved_errno == EACCES)) {
            return true;
        }

        if (written != sizeof(value)) {
            errno = saved_errno;
            return false;
        }

        return true;
    };

    auto cleanup_before_ready = [&]() ->bool {
        if (kill(supervisor_pid, SIGKILL) == -1 &&
            errno != ESRCH) {
            // unrecoverable cleanup failure
        }

        int supervisor_status = 0;

        pid_t waited;
        do {
            waited = waitpid(
                supervisor_pid,
                &supervisor_status,
                0
            );
        } while (waited == -1 && errno == EINTR);
        const bool execution_killed = kill_execution_cgroup();
        if (!execution_killed) {
            close(control_socket[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            return false;
        }
        
        constexpr int cgroup_wait_ms = 1000;
        constexpr int cgroup_poll_ms = 10;

        int elapsed_ms = 0;

        while (elapsed_ms < cgroup_wait_ms) {
            if (is_execution_cgroup_empty()) {
                break;
            }

            usleep(cgroup_poll_ms * 1000);
            elapsed_ms += cgroup_poll_ms;
        }
        if (!is_execution_cgroup_empty()) {
            close(control_socket[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            return false;
           }
            
        if (!cleanup_execution_cgroup()) {
            close(control_socket[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            return false;
        }

        close(control_socket[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        
        return  true;
    };
    SandboxMessageHeader header{};
    SandboxReadyPayload ready_payload{};

    const bool received = receive_message(
        control_socket[0],
        header,
        &ready_payload,
        sizeof(ready_payload)
    );

    if (!received) {
        cleanup_before_ready();

        result.state = ExecutionState::SandboxFailed;
        result.status = ExecutionStatus::SandboxFailure;
        return result;
        
    }

    if (header.type == SandboxMessageType::SetupFailed) {
        close(control_socket[0]);
        int supervisor_status = 0;

        pid_t waited;
        do {
            waited = waitpid(
                supervisor_pid,
                &supervisor_status,
                0
            );
        } while (waited == -1 && errno == EINTR);

        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        result.state = ExecutionState::SandboxFailed;
        result.status = ExecutionStatus::SandboxFailure;
        return result;
    }

    if(header.type != SandboxMessageType::Ready) {
        cleanup_before_ready();
        result.state = ExecutionState::RunnerFailed;
        result.status = ExecutionStatus::RunnerFailure;
        return result;
    }
    const pid_t  submission_pid = ready_payload.host_pid;
    const pid_t namespace_pid = ready_payload.namespace_pid;
    

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    close(stdin_pipe[0]);
    

    pollfd fds[4];
    fds[0] = {stdin_pipe[1],POLLOUT,0};
    fds[1] = {stdout_pipe[0],POLLIN,0};
    fds[2] = {stderr_pipe[0],POLLIN,0};
    fds[3] = {control_socket[0],POLLIN,0};

    char buffer_out[IO_BUFFER_SIZE];
    char buffer_err[IO_BUFFER_SIZE];
    bool stdin_open = true;
    bool stdout_open = true; 
    bool stderr_open = true;
    bool termination_requested = false;
    bool control_channel_open = true;
    bool supervisor_reaped = false;
    bool submission_finished = false;
  

    std::string stdout_output="";
    std::string stderr_output="";
    size_t input_offset = 0;
    int status=0;
    pid_t wait_result;
    struct rusage usage{};
    int ready;

    
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

    auto cleanup_after_failure = [&]() -> bool {
        bool containment_failed = false;
        bool execution_contained = false;
        if(stdin_open) {
            close(stdin_pipe[1]);
            stdin_open = false;
            fds[0].fd = -1;
        }

        if(control_channel_open) {
            const bool sent = send_message(control_socket[0], SandboxMessageType::Terminate,nullptr,0);
            if(sent) {
                termination_requested = true;
            }
            else{
                close(control_socket[0]);
                control_channel_open = false;
                fds[3].fd = -1;
            }
        }
        
        if(supervisor_pid > 0) {
           int supervisor_status = 0;
           bool supervisor_reaped = false;

           constexpr int cleanup_wait_ms = 1000;
           constexpr int cleanup_poll_ms = 10;
           
           int elapsed_ms = 0;

           while(elapsed_ms < cleanup_wait_ms) {
                pid_t waited;

                do{
                    waited =  waitpid(supervisor_pid,&supervisor_status,WNOHANG);
                } while(waited == -1 && errno == EINTR);

                if(waited == supervisor_pid) {
                    supervisor_reaped = true;
                    break;
                }
                if(waited == -1) {
                    break;
                }
                
                usleep(cleanup_poll_ms * 1000);
                elapsed_ms += cleanup_poll_ms;
           }
            const bool execution_killed = kill_execution_cgroup();

            if (!execution_killed) {
                containment_failed = true;
            }
            else {
                constexpr int cgroup_wait_ms = 1000;
                constexpr int cgroup_poll_ms = 10;

                int elapsed_ms = 0;

                while (elapsed_ms < cgroup_wait_ms) {
                    if (is_execution_cgroup_empty()) {
                        break;
                    }

                    usleep(cgroup_poll_ms * 1000);
                    elapsed_ms += cgroup_poll_ms;
                }

                if (is_execution_cgroup_empty()) {
                    execution_contained = true;
                }
                else {
                    containment_failed = true;
                }
            }

            if (!supervisor_reaped) {

                if (kill(supervisor_pid, SIGKILL) == -1 &&
                    errno != ESRCH) {
                    // Cannot do anything stronger at this layer.
                }
                pid_t waited;
                do {
                    waited =
                        waitpid(
                            supervisor_pid,
                            &supervisor_status,
                            0
                        );
                } while (waited == -1 && errno == EINTR);
                 if (waited == supervisor_pid) {
                    supervisor_reaped = true;
                 
                }

                if (waited == -1) {
                    containment_failed = true;
                 
                }
            }

            if (supervisor_reaped && execution_contained) {
                if (!cleanup_execution_cgroup()) {
                    containment_failed = true;
                }
            }
            
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

        if (control_channel_open) {
            close(control_socket[0]);
            control_channel_open = false;
            fds[3].fd = -1;
        }
        return !containment_failed;
    };

    auto finish_supervisor_terminal = [&]() -> bool {
        if (control_channel_open) {
            close(control_socket[0]);
            control_channel_open = false;
            fds[3].fd = -1;
        }

        int supervisor_status = 0;

        pid_t waited;
        do {
            waited = waitpid(
                supervisor_pid,
                &supervisor_status,
                0
            );
        } while (waited == -1 && errno == EINTR);

        if (waited != supervisor_pid) {
            return false;
        }
        supervisor_reaped = true;
        return true;
    };
   
    if(input.size() == 0){
            stdin_open=false;
            close(stdin_pipe[1]);
           
            fds[0].fd = -1;
    }

    while (!submission_finished || stdin_open ||stdout_open || stderr_open ) {
        const auto now = std::chrono::steady_clock::now();
        if(!submission_finished && !termination_requested && now>=deadline) {
            const bool sent = send_message(control_socket[0],SandboxMessageType::Terminate,nullptr,0);

            if(!sent) {
                cleanup_after_failure();
                return result;
            }

            termination_requested = true;
            result.wall_time_limit_exceeded = true;
            if(stdin_open) {
                close(stdin_pipe[1]);
                stdin_open = false; 
                fds[0].fd = -1;
            }
        }

       
        int timeout = 0;
        

        if(!submission_finished) {
            if(termination_requested) {
                timeout = 10;
            }
            else{
                if( now >= deadline){
                    timeout = 0;
                }
                else{
                    const auto remaining = std::chrono::duration_cast <std::chrono::milliseconds>(deadline - now); 
                    timeout = static_cast<int>(remaining.count());
                }
            }
        }
        
        if (submission_finished &&
            !stdin_open &&
            !stdout_open &&
            !stderr_open) {
            break;
        }
        do {
              
            ready = poll(fds, 4, timeout);
           
        } while (ready == -1 && errno == EINTR);

        if (ready == -1) {
            const bool cleanup_ok = cleanup_after_failure();
            result.status = cleanup_ok
                ? ExecutionStatus::RunnerFailure
                : ExecutionStatus::SandboxFailure;
            return result;
        }

            
        if (control_channel_open &&
            (fds[3].revents & (POLLIN | POLLERR | POLLHUP))) {

            SandboxMessageHeader header{};

            if (!read_all(
                    control_socket[0],
                    &header,
                    sizeof(header))) {

                cleanup_after_failure();
                result.status = ExecutionStatus::RunnerFailure;
                return result;
            }

            if (header.type == SandboxMessageType::SetupFailed) {

                SandboxSetupFailedPayload failure{};

                if (header.payload_size != sizeof(failure)) {
                    cleanup_after_failure();
                    result.status = ExecutionStatus::RunnerFailure;
                    return result;
                }

                if (!read_all(
                        control_socket[0],
                        &failure,
                        sizeof(failure))) {

                    const bool cleanup_ok = cleanup_after_failure();
                    result.status = cleanup_ok
                        ? ExecutionStatus::RunnerFailure
                        : ExecutionStatus::SandboxFailure;
                    return result;
                }

                if (!finish_supervisor_terminal()) {
                    close_pipes();
                    result.status = ExecutionStatus::SandboxFailure;
                    return result;
                }
                close_pipes();
                result.status = ExecutionStatus::SandboxFailure;
                return result;

                
            }
            else if (header.type == SandboxMessageType::Terminated) {

                SandboxTerminatedPayload terminated{};

                if (header.payload_size != sizeof(terminated)) {
                    cleanup_after_failure();
                    result.status = ExecutionStatus::RunnerFailure;
                    return result;
                }

                if (!read_all(
                        control_socket[0],
                        &terminated,
                        sizeof(terminated))) {

                    const bool cleanup_ok = cleanup_after_failure();
                    result.status = cleanup_ok
                        ? ExecutionStatus::RunnerFailure
                        : ExecutionStatus::SandboxFailure;
                    return result;
                }

                status = terminated.wait_status;
                usage = terminated.usage;

                submission_finished = true;
                if (!finish_supervisor_terminal()) {
                    close_pipes();
                    result.state = ExecutionState::RunnerFailed;
                    result.status = ExecutionStatus::RunnerFailure;
                    return result;
                }
                finalize_resource_usage();
                finalize_status();
                close_pipes();
                
            }
            else {
                cleanup_after_failure();
                result.status = ExecutionStatus::RunnerFailure;
                return result;
            }
        }
        
        
        if(stdin_open && (fds[0].revents & (POLLHUP | POLLERR))) {
            close(stdin_pipe[1]);
            stdin_open = false;
            fds[0].fd = -1;
        }
        else if(stdin_open && (fds[0].revents & POLLOUT )){

            std::size_t remaining = input.size() - input_offset;
            std::size_t chunk_size = std::min(remaining, IO_BUFFER_SIZE);
            ssize_t bytes_written;
            do {
                bytes_written = write(stdin_pipe[1],input.data() + input_offset,chunk_size);
            } while (bytes_written == -1 && errno == EINTR);

            if (bytes_written == -1) {
                if(errno == EPIPE){
                    close(stdin_pipe[1]);
                    stdin_open = false;
                    fds[0].fd = -1;
                }
                else{
                    const bool cleanup_ok = cleanup_after_failure();
                    result.status = cleanup_ok
                        ? ExecutionStatus::RunnerFailure
                        : ExecutionStatus::SandboxFailure;
                    return result;
                }
                
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
                    const bool cleanup_ok = cleanup_after_failure();
                    result.status = cleanup_ok
                        ? ExecutionStatus::RunnerFailure
                        : ExecutionStatus::SandboxFailure;
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
                    const bool cleanup_ok = cleanup_after_failure();
                    result.status = cleanup_ok
                        ? ExecutionStatus::RunnerFailure
                        : ExecutionStatus::SandboxFailure;
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
    if (!supervisor_reaped) {
        int supervisor_status = 0;

        pid_t reaped_supervisor;
        do {
            reaped_supervisor =
                waitpid(supervisor_pid, &supervisor_status, 0);
        } while (reaped_supervisor == -1 && errno == EINTR);

        if (reaped_supervisor == -1) {
            const bool cleanup_ok = cleanup_after_failure();
            result.status = cleanup_ok
                ? ExecutionStatus::RunnerFailure
                : ExecutionStatus::SandboxFailure;
            return result;
        }

        supervisor_reaped = true;
    }

   
    if (result.wall_time_limit_exceeded) {
        result.state = ExecutionState::TimedOut;
        result.status = ExecutionStatus::TimedOut;
    }
    else if (WIFSIGNALED(status)) {
        result.state = ExecutionState::Signaled;
        result.status = ExecutionStatus::Signaled;
    }
    else {
        result.state = ExecutionState::Finished;
        result.status = ExecutionStatus::Completed;
    }
    result.stdout_output=stdout_output;
    result.stderr_output=stderr_output;
     
    return result;
}