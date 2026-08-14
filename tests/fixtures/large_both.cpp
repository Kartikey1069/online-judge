#include <iostream>
#include <string>

int main() {
    std::string stdout_data(10000, 'o');
    std::string stderr_data(10000, 'e');

    std::cout.write(stdout_data.data(), stdout_data.size());
    std::cerr.write(stderr_data.data(), stderr_data.size());

    return 0;
}