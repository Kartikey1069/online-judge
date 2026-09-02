#include <filesystem>
#include <iostream>
#include <string>
#include <chrono>

#include "runner/process_runner.hpp"
#include "common/execution_config.hpp"

int main(int argc, char* argv[]) {
    std::filesystem::path project_root = std::filesystem::current_path();

    std::filesystem::path executable_path = argc > 1
        ? std::filesystem::path(argv[1])
        : project_root / "build" / "fixture_print_stdout";

    if (!executable_path.is_absolute()) {
        executable_path = project_root / executable_path;
    }

    if (!std::filesystem::exists(executable_path)) {
        std::cerr << "Executable not found: " << executable_path << '\n';
        std::cerr << "Usage: oj_runner <path-to-binary>\n";
        std::cerr << "Example: ./oj_runner build/fixture_print_stdout\n";
        return 1;
    }

    const ExecutionLimits limits{
        .cpu_limit = std::chrono::seconds(1),
        .wall_limit = std::chrono::seconds(8),
        .memory_limit = std::size_t(1ULL * 1024 * 1024 * 1024)
    };

    const ExecutionConfig config{ .limit = limits };
    ProcessRunner runner;

    std::cout << "Online Judge started.\n";
    std::cout << "Running: " << executable_path << '\n';

    const ExecutionResult result = runner.run(
        executable_path.string(),
        {},
        "",
        config
    );

    std::cout << "exit_code=" << result.exit_code << '\n';
    std::cout << "status=" << static_cast<int>(result.status) << '\n';
    std::cout << "stdout:\n" << result.stdout_output;
    std::cout << "stderr:\n" << result.stderr_output;

    std::cout << "Judge Terminated.\n";
    return result.exit_code == 0 ? 0 : 1;
}