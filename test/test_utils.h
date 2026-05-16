#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ANSI Color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"

struct TestCase {
    std::string name;
    std::function<void()> testFunc;
};

inline std::vector<TestCase>& getTestRegistry() {
    static std::vector<TestCase> registry;
    return registry;
}

struct TestRegistrar {
    TestRegistrar(std::string name, std::function<void()> func) {
        getTestRegistry().push_back({name, func});
    }
};

#define TEST_CASE(name) \
    void name(); \
    TestRegistrar registrar_##name(#name, name); \
    void name()

// Assertions
#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << COLOR_RED << "  [FAIL] " << #condition << " is false at " << __FILE__ << ":" << __LINE__ << COLOR_RESET << std::endl; \
        throw std::runtime_error("Assertion failed"); \
    }

#define ASSERT_FALSE(condition) \
    if ((condition)) { \
        std::cerr << COLOR_RED << "  [FAIL] " << #condition << " is true at " << __FILE__ << ":" << __LINE__ << COLOR_RESET << std::endl; \
        throw std::runtime_error("Assertion failed"); \
    }

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        std::cerr << COLOR_RED << "  [FAIL] Expected " << (expected) << ", got " << (actual) << " at " << __FILE__ << ":" << __LINE__ << COLOR_RESET << std::endl; \
        throw std::runtime_error("Assertion failed"); \
    }

#define ASSERT_NE(expected, actual) \
    if ((expected) == (actual)) { \
        std::cerr << COLOR_RED << "  [FAIL] Expected not " << (expected) << ", but it was equal at " << __FILE__ << ":" << __LINE__ << COLOR_RESET << std::endl; \
        throw std::runtime_error("Assertion failed"); \
    }

#endif // TEST_UTILS_H
