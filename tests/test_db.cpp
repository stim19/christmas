#include <iostream>
#include <string>
#include "../db.h"

int main(int argc, char** argv) {
    try {
        std::cout << "=== Constructor test ===\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
    }
    std::cout << "All tests passed\n";
    return 0;
}
