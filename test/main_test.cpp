#include "test_utils.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "Running tests..." << std::endl;
    
    auto& registry = getTestRegistry();
    int passed = 0;
    int failed = 0;
    
    for (const auto& test : registry) {
        std::cout << "[RUN] " << test.name << "...";
        try {
            test.testFunc();
            std::cout << "\r" << COLOR_GREEN << "[PASS] " << test.name << COLOR_RESET << "          " << std::endl;
            passed++;
        } catch (const std::exception& e) {
            std::cout << "\r" << COLOR_RED << "[FAIL] " << test.name << COLOR_RESET << "          " << std::endl;
            failed++;
        } catch (...) {
            std::cout << "\r" << COLOR_RED << "[FAIL] " << test.name << " (Unknown error)" << COLOR_RESET << "          " << std::endl;
            failed++;
        }
    }
    
    std::cout << "\n" << "Test Summary: " << passed << " passed, " << failed << " failed." << std::endl;
    
    return (failed == 0) ? 0 : 1;
}

