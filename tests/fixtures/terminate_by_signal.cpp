#include <unistd.h>

int main() {
    // Sleep indefinitely. The test runner will kill this via wall time limit.
    while (true) {
        pause();
    }
    return 0;
}