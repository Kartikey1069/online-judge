#include <iostream>

int main() {
    const char data[] = {'a', '\0', 'b'};

    std::cout.write(data, sizeof(data));

    return 0;
}