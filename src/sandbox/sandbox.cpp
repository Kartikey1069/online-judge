#include "sandbox/sandbox.hpp"
#include <sched.h>
#include <cerrno>
#include <iostream>
#include <sys/mount.h>
#include <cstdlib>
#include <cerrno>
#include <string>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <signal.h>

Sandbox::Sandbox(const  SandboxConfig& config):config_(config){

}



SandboxSetupResult Sandbox::failSetup(int status_fd,SandboxSetupStage stage, int error_code) {
    const SandboxSetupFailedPayload payload{
        .failed_stage = stage,
        .error_code = error_code
    };

    send_message(
        status_fd,
        SandboxMessageType::SetupFailed,
        &payload,
        sizeof(payload)
    );

    SandboxSetupResult result;
    result.success = false;
    result.failed_stage = stage;
    result.error_code = error_code;

    return result;
}


SandboxSetupResult Sandbox::setupUserNamespace(int control_fd) {
    const uid_t host_uid = getuid();
    const gid_t host_gid = getgid();
    if(unshare(CLONE_NEWUSER) == -1) {
        const int error_code = errno;

        return  failSetup(control_fd,SandboxSetupStage::Identity,error_code);
    }
    
    std::cerr << "after unshare: uid=" << getuid()
          << " gid=" << getgid() << '\n';

    const std::string uid_map = "0 " + std::to_string(host_uid) + " 1\n";
    const std::string gid_map = "0 " + std::to_string(host_gid) + " 1\n";

    const int setgroups_fd = open("/proc/self/setgroups", O_WRONLY);

    if(setgroups_fd == -1){
        const int error_code = errno;

        return  failSetup(control_fd,SandboxSetupStage::Identity,error_code);
    }

    const char deny[] = "deny\n";
    ssize_t written_setgroups = write(setgroups_fd , deny , sizeof(deny)-1);

    if(written_setgroups != static_cast<ssize_t>(sizeof(deny)-1)) {
        const int error_code = errno;
        
        close(setgroups_fd);
        return  failSetup(control_fd,SandboxSetupStage::Identity,error_code);
        
    }
    close(setgroups_fd);

    const int uid_fd = open("/proc/self/uid_map", O_WRONLY);

    if(uid_fd == -1) {
        const int error_code = errno;

        return  failSetup(control_fd,SandboxSetupStage::Identity,error_code);
    }
    ssize_t written = write(uid_fd , uid_map.data() , uid_map.size());

    if(written != static_cast<ssize_t>(uid_map.size())) {
        const int error_code = errno;
        
        close(uid_fd);
        return  failSetup(control_fd,SandboxSetupStage::Identity,error_code);
        
    }
    close(uid_fd);

    const int gid_fd = open("/proc/self/gid_map", O_WRONLY);

    if(gid_fd == -1) {
        const int error_code = errno;

        return  failSetup(control_fd,SandboxSetupStage::Identity,error_code);
    }
    ssize_t written_gid = write(gid_fd , gid_map.data() , gid_map.size());

    if(written_gid != static_cast<ssize_t>(gid_map.size())) {
        const int error_code = errno;
        
        close(gid_fd);
        return  failSetup(control_fd,SandboxSetupStage::Identity,error_code);
    }
    close(gid_fd);

    SandboxSetupResult result;
    result.success = true;
    return result;
}

SandboxSetupResult  Sandbox::setupCgroup(int control_fd,std::uint64_t execution_id) {
    cgroup_path_ = "/sys/fs/cgroup/online-judge/execution-" + std::to_string(execution_id);

    if (mkdir(cgroup_path_.c_str(), 0755) == -1) {
        const int error_code = errno;

        if (error_code == EEXIST) {
            cgroup_path_.clear();
            return failSetup(control_fd, SandboxSetupStage::Cgroup, error_code);
        }

        if (error_code == EPERM || error_code == EACCES) {
            cgroup_path_.clear();
            SandboxSetupResult result;
            result.success = true;
            return result;
        }

        return failSetup(control_fd, SandboxSetupStage::Cgroup, error_code);
    }

    SandboxSetupResult  result;
    result.success = true;
    return result;
}


SandboxSetupResult Sandbox::prepareEnvironment(int control_fd,std::uint64_t execution_id)
{   SandboxSetupResult  cgroup_result = setupCgroup(control_fd,execution_id);

    if(!cgroup_result.success) {
        return cgroup_result;
    } 

    SandboxSetupResult result;
    char template_path[] = "/tmp/oj-sandbox-XXXXXX";

    char* path = mkdtemp(template_path);

    if(path == nullptr) {
        const int error_code = errno;

        return failSetup(control_fd,SandboxSetupStage::Filesystem,error_code);
    }

    root_path_ = path;

    
    
    result.success = true;
    return result;
}

SandboxProcess Sandbox::createExecutionProcess(
    const SandboxExecutionContext& context,
    const SandboxExecutionSpec& spec)
{
    int sync_pipe[2];

    if (pipe(sync_pipe) == -1) {
        return {};
    }

    int status_pipe[2];

    if (pipe(status_pipe) == -1) {
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        return {};
    }

    int flags = fcntl(status_pipe[1], F_GETFD);

    if (flags == -1) {
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        close(status_pipe[0]);
        close(status_pipe[1]);
        return {};
    }

    if (fcntl(status_pipe[1], F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        close(status_pipe[0]);
        close(status_pipe[1]);
        return {};
    }

    SandboxProcess process{};

    pid_t pid = fork();

    if (pid == -1) {
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        close(status_pipe[0]);
        close(status_pipe[1]);
        return process;
    }

    auto cleanup_failed_child = [&](pid_t child_pid) -> bool {
        if (kill(child_pid, SIGKILL) == -1 && errno != ESRCH) {
        }

        int child_status = 0;

        pid_t waited;

        do {
            waited = waitpid(child_pid, &child_status, 0);
        } while (waited == -1 && errno == EINTR);

        close(sync_pipe[1]);
        close(status_pipe[0]);

        return waited == child_pid;
    };

    // ------------------------------------------------------------
    // CHILD
    // ------------------------------------------------------------

    if (pid == 0) {
        close(sync_pipe[1]);
        close(status_pipe[0]);

        char signal = 0;
        ssize_t bytes_read;

        do {
            bytes_read = read(
                sync_pipe[0],
                &signal,
                sizeof(signal));

        } while (bytes_read == -1 && errno == EINTR);

        close(sync_pipe[0]);

        if (bytes_read != 1) {
            _exit(EXIT_FAILURE);
        }

        SandboxExecutionContext execution_context = context;
        execution_context.status_fd = status_pipe[1];

        // 1. Enter user namespace
        SandboxSetupResult result =
            setupUserNamespace(execution_context.control_fd);

        if (!result.success) {
            _exit(EXIT_FAILURE);
        }

        // 2. Create mount namespace
        if (unshare(CLONE_NEWNS) == -1) {
            const int error_code = errno;

            failSetup(
                execution_context.status_fd,
                SandboxSetupStage::Filesystem,
                error_code);

            _exit(EXIT_FAILURE);
        }

        // 3. Stop mount propagation between this namespace and parent
        if (mount(
                nullptr,
                "/",
                nullptr,
                MS_REC | MS_PRIVATE,
                nullptr) == -1) {

            const int error_code = errno;

            failSetup(
                execution_context.status_fd,
                SandboxSetupStage::Filesystem,
                error_code);

            _exit(EXIT_FAILURE);
        }

        if(mount(root_path_.c_str(),root_path_.c_str(),nullptr,MS_BIND | MS_REC,nullptr) == -1) {
            const int error_code = errno;

            failSetup(execution_context.control_fd,SandboxSetupStage::Filesystem,error_code);
            _exit(EXIT_FAILURE);
        }

        // 4. Create PID namespace
        if (unshare(CLONE_NEWPID) == -1) {
            const int error_code = errno;

            failSetup(
                execution_context.status_fd,
                SandboxSetupStage::Namespace,
                error_code);

            _exit(EXIT_FAILURE);
        }

        // Fork Child B to enter the new PID namespace
        pid_t child_b = fork();

        if (child_b == -1) {
            const int error_code = errno;
            failSetup(
                execution_context.status_fd,
                SandboxSetupStage::Namespace,
                error_code);
            _exit(EXIT_FAILURE);
        }

        if (child_b > 0) {
            // Child A waits for Child B, then forwards its wait_status
            // through the status pipe before exiting.
            //
            // WHY: On WSL2, tgkill(1, 1, SIGKILL) inside a user-namespace
            // PID namespace is intercepted by the kernel and converted into
            // exit_group(0). As a result, waitpid() on Child B returns
            // WIFEXITED(0), not WIFSIGNALED(SIGKILL), losing the signal info.
            //
            // The status pipe write end (execution_context.status_fd) is
            // tagged with CLOEXEC and is held by Child A only. We write
            // the raw b_status (as a tagged WaitStatus record) before closing,
            // so the Supervisor can read the real terminal cause.

            int b_status = 0;
            pid_t waited;

            do {
                waited = waitpid(child_b, &b_status, 0);
            } while (waited == -1 && errno == EINTR);

            if (waited == child_b) {
                // Forward the raw wait_status so supervisor sees the truth.
                write_wait_status(execution_context.status_fd, b_status);
            }

            // Close the status pipe write end (signals EOF to supervisor).
            close(execution_context.status_fd);

            if (waited == child_b) {
                if (WIFEXITED(b_status)) {
                    _exit(WEXITSTATUS(b_status));
                } else if (WIFSIGNALED(b_status)) {
                    int sig = WTERMSIG(b_status);
                    sigset_t set;
                    sigemptyset(&set);
                    sigaddset(&set, sig);
                    sigprocmask(SIG_UNBLOCK, &set, nullptr);
                    struct sigaction sa;
                    sa.sa_handler = SIG_DFL;
                    sigemptyset(&sa.sa_mask);
                    sa.sa_flags = 0;
                    sigaction(sig, &sa, nullptr);
                    kill(getpid(), sig);
                    while(true) pause();
                }
            }
            _exit(EXIT_FAILURE);
        }

        // Child B (PID 1 in the new PID namespace)

        // 5. Finish filesystem construction
        result =
            finalizeExecutionEnvironment(execution_context, spec);

        if (!result.success) {
            _exit(EXIT_FAILURE);
        }

        // 6. Execute submission
        execute(execution_context, spec);
    }

    // ------------------------------------------------------------
    // PARENT / SUPERVISOR
    // ------------------------------------------------------------

    close(sync_pipe[0]);
    close(status_pipe[1]);

    SandboxSetupResult cgroup_result =
        addProcessToCgroup(pid);

    if (!cgroup_result.success) {
        cleanup_failed_child(pid);
        return {};
    }

    const char signal = 1;
    ssize_t bytes_written;

    do {
        bytes_written = write(
            sync_pipe[1],
            &signal,
            sizeof(signal));

    } while (bytes_written == -1 && errno == EINTR);

    if (bytes_written != 1) {
        cleanup_failed_child(pid);
        return {};
    }

    close(sync_pipe[1]);

    process.host_pid = pid;
    process.namespace_pid = 1;
    process.status_fd = status_pipe[0];

    return process;
}

SandboxSetupResult Sandbox::finalizeExecutionEnvironment(const SandboxExecutionContext& context, const SandboxExecutionSpec& spec)
{
    SandboxSetupResult result;
    
    // Bind mount required directories for dynamically linked executables
    const char* dirs[] = {"/lib", "/lib64", "/usr", "/bin"};
    for (const char* dir : dirs) {
        std::string target = root_path_ + dir;
        mkdir(target.c_str(), 0755);
        if (mount(dir, target.c_str(), nullptr, MS_BIND | MS_REC, nullptr) != -1) {
            mount(nullptr, target.c_str(), nullptr, MS_BIND | MS_REMOUNT | MS_RDONLY | MS_REC, nullptr);
        }
    }

    // Bind mount the executable
    std::string exe_target = root_path_ + "/sandbox_exec";
    int fd = creat(exe_target.c_str(), 0755);
    if (fd != -1) {
        close(fd);
        if (mount(spec.executable_path.c_str(), exe_target.c_str(), nullptr, MS_BIND, nullptr) == -1) {
            return failSetup(context.status_fd, SandboxSetupStage::Filesystem, errno);
        }
        mount(nullptr, exe_target.c_str(), nullptr, MS_BIND | MS_REMOUNT | MS_RDONLY, nullptr);
    } else {
        return failSetup(context.status_fd, SandboxSetupStage::Filesystem, errno);
    }

     const std::string old_root = root_path_ + "/old_root";

    if(mkdir(old_root.c_str(), 0700) == -1){
        const int error_code = errno;

        return failSetup(context.status_fd,SandboxSetupStage::Filesystem,error_code);
    }

    if(syscall(SYS_pivot_root,root_path_.c_str(),old_root.c_str()) == -1) {
        const int error_code = errno;

        return failSetup(context.status_fd,SandboxSetupStage::Filesystem,error_code);
    }

    if (chdir("/") == -1) {
        const int error_code = errno;

        return failSetup(context.status_fd,SandboxSetupStage::Filesystem,error_code);
    }

    if(umount2("/old_root",MNT_DETACH) == -1){
        const int error_code = errno;

        return failSetup(context.status_fd,SandboxSetupStage::Filesystem,error_code);
    }

    if(rmdir("/old_root") == -1) {
        const int error_code = errno;

        return failSetup(context.status_fd,SandboxSetupStage::Filesystem,error_code);
    }

    if (mkdir("/proc", 0555) == -1) {
        const int error_code = errno;

        return failSetup(context.status_fd,SandboxSetupStage::Filesystem,error_code);
    }

    if(mount("proc","/proc","proc",0,nullptr) == -1){
        const int error_code = errno;

        return failSetup(context.status_fd,SandboxSetupStage::Filesystem,error_code);
    }
    result.success = true ;
    return result ;
}

bool Sandbox::terminate() {
    if(cgroup_path_.empty()) {
        return true;
    }

    const std::string kill_path = cgroup_path_ + "/cgroup.kill";

    const int fd = open(kill_path.c_str(),O_WRONLY);

    if(fd == -1) {
        if (errno == EPERM || errno == EACCES || errno == ENOENT) {
            cgroup_path_.clear();
            return true;
        }
        return false;
    }
    const char value = '1';

    const ssize_t  written  =  write(fd,&value ,1);
    
    close(fd);
    if (written == -1 && (errno == EPERM || errno == EACCES)) {
        cgroup_path_.clear();
        return true;
    }
    return written == 1;
}

bool Sandbox::cleanupCgroup() {
    if (cgroup_path_.empty()) {
        return true;
    }

    int retries = 5;
    while (retries > 0) {
        if (rmdir(cgroup_path_.c_str()) == -1) {
            if (errno == ENOENT) {
                cgroup_path_.clear();
                return true;
            } else if (errno == EBUSY) {
                // The cgroup state takes a moment to propagate
                usleep(10000); // 10ms
                retries--;
            } else if (errno == EPERM || errno == EACCES) {
                cgroup_path_.clear();
                return true;
            } else {
                return false;
            }
        } else {
            cgroup_path_.clear();
            return true;
        }
    }

    return false;
}

SandboxSetupResult Sandbox::addProcessToCgroup(pid_t pid) {
    SandboxSetupResult result ;

    if(cgroup_path_.empty()) {
        result.success = true;
        return result;
    }
    const std::string  procs_path = cgroup_path_ + "/cgroup.procs";

    const int fd = open(procs_path.c_str(),O_WRONLY);

    if(fd == -1) {
        if (errno == EPERM || errno == EACCES) {
            cgroup_path_.clear();
            result.success = true;
            return result;
        }
        result.failed_stage = SandboxSetupStage::Cgroup;
        result.error_code = errno;
        return result;
    }
    const std::string pid_string = std::to_string(pid);

    const ssize_t  written = write(fd,pid_string.data(),pid_string.size());
    const int error_code = (written == -1) ? errno : EIO;

    close(fd);

    if (written == -1 && (errno == EPERM || errno == EACCES)) {
        cgroup_path_.clear();
        result.success = true;
        return result;
    }

    if(written != static_cast<ssize_t>(pid_string.size())) {
        result.failed_stage = SandboxSetupStage::Cgroup;
        result.error_code = error_code;
        return result;
    }
    result.success = true;
    return result;
}
[[noreturn]] void Sandbox::execute(const SandboxExecutionContext& context, const SandboxExecutionSpec& spec) {
    
    auto report_failure = [&](SandboxSetupStage stage, int error_code) {
        SandboxSetupFailedPayload payload { 
            .failed_stage = stage,
            .error_code = error_code 
        };

        write_all(context.status_fd,&payload,sizeof(payload));

    };
    
    if(dup2(context.stdin_fd , STDIN_FILENO) == -1){
        const int error_code = errno;
        report_failure (SandboxSetupStage::StandardIO,error_code);
        _exit(EXIT_FAILURE);
    }
    if (dup2(context.stdout_fd, STDOUT_FILENO) == -1) {
        const int error_code = errno;
        report_failure (SandboxSetupStage::StandardIO,error_code);
        _exit(EXIT_FAILURE);
    }

    if (dup2(context.stderr_fd, STDERR_FILENO) == -1) {
        const int error_code = errno;
        report_failure (SandboxSetupStage::StandardIO,error_code);
        _exit(EXIT_FAILURE);
    }

    close(context.stdin_fd);
    close(context.stdout_fd);
    close(context.stderr_fd);
    
    struct rlimit cpu_limit{};

    cpu_limit.rlim_cur = spec.cpu_limit.count();
    cpu_limit.rlim_max = spec.cpu_limit.count();

    if(setrlimit(RLIMIT_CPU, &cpu_limit) == -1) {
        const int error_code = errno; 
        report_failure(SandboxSetupStage::ResourceLimit,error_code);
        _exit(EXIT_FAILURE);
    }

    struct rlimit memory_limit{};

    memory_limit.rlim_cur = spec.memory_limit;
    memory_limit.rlim_max = spec.memory_limit;

    if (setrlimit(RLIMIT_AS, &memory_limit) == -1) {
        const int error_code = errno; 
        report_failure(SandboxSetupStage::ResourceLimit,error_code);
        _exit(EXIT_FAILURE);
    }
    std::vector<char*> argv;

    argv.reserve(spec.args.size() + 2);

    argv.push_back(
        const_cast<char*>("/sandbox_exec")
    );

    for (const auto& arg : spec.args) {
        argv.push_back(
            const_cast<char*>(arg.c_str())
        );
    }

    argv.push_back(nullptr);
    execvp("/sandbox_exec",argv.data());
    const int error_code = errno; 
    report_failure(SandboxSetupStage::Exec,error_code);
    _exit(EXIT_FAILURE);
}

bool Sandbox::isCgroupEmpty() const {
    const std::string path = cgroup_path_ + "/cgroup.events";
    const int fd = open(path.c_str(), O_RDONLY);

        if (fd == -1) {
            return false;
        }

        char buffer[256];
        ssize_t bytes_read;

        do {
            bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        } while (bytes_read == -1 && errno == EINTR);

        close(fd);

        if (bytes_read == -1) {
            return false;
        }

        buffer[bytes_read] = '\0';

        const char* populated = std::strstr(buffer, "populated ");

        if (populated == nullptr) {
            return false;
        }

        populated += std::strlen("populated ");

        return *populated == '0';
}
