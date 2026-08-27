

#include "compiler/compiler.hpp"

#include "runner/process_runner.hpp"



#include <vector>

CompileResult Compiler::compile(const std::string& source_file,const std::string& output_file){
        ProcessRunner runner;
        const std::vector<std::string>args={source_file,"-o",output_file};


        ExecutionLimits compile_limits{
            .cpu_limit = std::chrono::seconds(30),
            .wall_limit = std::chrono::seconds(60),
            .memory_limit = 1ULL * 1024 * 1024 * 1024
        };
         
        ExecutionConfig config;
        config.limit = compile_limits;
        ExecutionResult execute = runner.run("g++",args,"",config);
        CompileResult result;
        result.exit_code = execute.exit_code; 
        result.stderr_output = execute.stderr_output;
        if(result.exit_code == 0){
            result.executable_path = output_file;
        }
        return result;
}