

#include "compiler/compiler.hpp"

#include "runner/process_runner.hpp"



#include <vector>

CompileResult Compiler::compile(const std::string& source_file,const std::string& output_file){
        ProcessRunner runner;
        const std::vector<std::string>args={source_file,"-o",output_file};
        ExecutionResult execute = runner.run("g++",args);
        CompileResult result;
        result.exit_code = execute.exit_code; 
        result.stderr_output = execute.stderr_output;
        if(result.exit_code == 0){
            result.executable_path = output_file;
        }
        return result;
}