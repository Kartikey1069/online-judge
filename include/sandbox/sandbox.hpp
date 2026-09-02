#pragma once

#include "common/sandbox_config.hpp"
#include "common/sandbox_protocol.hpp"
#include <cstdint>
#include <sys/types.h>
#include <string>
#include <vector>
#include <chrono>
#include <cstddef>

struct SandboxSetupResult {
    bool success = false;
    SandboxProcess process{};
    SandboxSetupStage failed_stage = SandboxSetupStage::None;
    int error_code = 0;
};

struct SandboxExecutionContext {
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
    int control_fd;
    int status_fd;
};
struct SandboxExecutionSpec {
    const std::string& executable_path;
    const std::vector<std::string>& args;

    std::chrono::seconds cpu_limit;
    std::size_t memory_limit;
};
class Sandbox {
public:
    explicit Sandbox(const SandboxConfig& config);

    SandboxSetupResult prepareEnvironment(int control_fd, std::uint64_t execution_id);

    SandboxProcess createExecutionProcess(const SandboxExecutionContext& context,const SandboxExecutionSpec& spec);

    SandboxSetupResult finalizeExecutionEnvironment(const SandboxExecutionContext& context, const SandboxExecutionSpec& spec);

    bool terminate();
    bool cleanupCgroup();
    bool cleanupMounts();
    bool cleanupRoot();
    SandboxSetupResult addProcessToCgroup(pid_t pid);

private:
    const SandboxConfig& config_;

    std::string root_path_;
    std::string cgroup_path_;
    
    SandboxSetupResult setupUserNamespace(int control_fd);
    SandboxSetupResult setupCgroup(int control_fd,std::uint64_t execution_id);
    SandboxSetupResult failSetup(int status_fd,SandboxSetupStage stage, int error_code);
    bool isCgroupEmpty() const;
    [[noreturn]] void execute(const SandboxExecutionContext& context, const SandboxExecutionSpec& spec);
};