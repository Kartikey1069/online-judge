#pragma once 

#include "compiler/compile_result.hpp"

#include <string>

class Compiler{
    public:
        CompileResult compile(const std::string& source_file,const std::string& output_file);

};