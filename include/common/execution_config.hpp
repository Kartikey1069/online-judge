#pragma once  

#include "common/execution_limits.hpp"

#include "common/sandbox_config.hpp"

struct  ExecutionConfig {
    ExecutionLimits limit;
    SandboxConfig sandbox;

    bool operator==(const ExecutionConfig&) const = default;
};
