#pragma once 

#include <chrono>

struct ExecutionLimits { 
    std::chrono::seconds cpu_limit;
    std::chrono::milliseconds wall_limit;
    std::size_t memory_limit;

    bool operator==(const ExecutionLimits&) const = default;
};
