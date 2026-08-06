#pragma once 

#include <ostream>

enum class Verdict
{
    Accepted,
    WrongAnswer,
    RuntimeError,
    TimeLimitExceeded,
    MemoryLimitExceeded,
    CompilationError,
    InternalError
};

std::string to_string(Verdict verdict);

std::ostream& operator<<(std::ostream&, Verdict);