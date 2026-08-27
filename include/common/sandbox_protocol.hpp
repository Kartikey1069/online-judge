#pragma once

#include <cstdint>
#include <sys/types.h>
#include <sys/resource.h>

enum class SandboxSetupStage {
    None,
    Namespace,
    Filesystem,
    Identity,
    Cgroup,
    Capabilities,
    Seccomp,
    ProcessCreation,
    StandardIO,
    ResourceLimit,
    Exec
};

enum class SandboxMessageType {
    Ready,
    SetupFailed,
    Terminated,
    Terminate
};
enum class StatusPipeReadResult {
    Payload,
    Eof,
    Error
};

struct SandboxMessageHeader {
    SandboxMessageType type;
    std::uint32_t payload_size;
};

struct SandboxReadyPayload {
    pid_t host_pid;
    pid_t namespace_pid;
    std::uint64_t execution_id;
};

struct SandboxProcess {
    pid_t host_pid = -1;
    pid_t namespace_pid = 1;
    int status_fd = -1;
};

struct SandboxTerminatedPayload {
    int wait_status;
    struct rusage usage;
};

struct SandboxSetupFailedPayload {
    SandboxSetupStage failed_stage;
    int error_code;
};


bool send_message(
    int fd,
    SandboxMessageType type,
    const void* payload,
    std::uint32_t payload_size);

bool receive_message(
    int fd,
    SandboxMessageHeader& header,
    void* payload,
    std::uint32_t payload_capacity);

bool read_all(int fd, void* buffer, std::size_t size);
bool write_all(int fd, const void* buffer, std::size_t size);
StatusPipeReadResult read_setup_failure(int fd,SandboxSetupFailedPayload&);