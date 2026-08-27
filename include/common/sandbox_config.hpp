#pragma once 

#include <cstddef>

enum class  NetworkPolicy {
    Disabled,
    LoopbackOnly, 
    Enabled
};
  

enum class  FileSystemPolicy { 
    Restricted
};

enum class SyscallPolicy {
    Standard
};

struct  SandboxConfig {
    NetworkPolicy network_policy = NetworkPolicy::Disabled;
    FileSystemPolicy filesystem_policy = FileSystemPolicy::Restricted;
    SyscallPolicy syscall_policy = SyscallPolicy::Standard;

    std::size_t max_processes = 20;
       bool operator==(const SandboxConfig&) const = default;
};