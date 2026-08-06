#pragma once 

#include <string>

#include "common/verdict.hpp"


struct JudgeResult
{
    Verdict verdict;

    std::string message;
};