#pragma once 

#include "submission/submission_result.hpp"
#include <string>


class SubmissionService{

    public:
       SubmissionResult evaluate(const std::string& solution_path,const std::string& expected_output);
};