#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <chrono>

namespace netcore::testing {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> func;
};

class TestRegistry {
public:
    static TestRegistry& getInstance() {
        static TestRegistry instance;
        return instance;
    }

    void registerTest(const std::string& suite, const std::string& name, std::function<void()> func) {
        tests_.push_back({suite, name, std::move(func)});
    }

    int runAll() {
        int passed = 0;
        int failed = 0;
        std::cout << "\n================ Running NetCore Test Suite ================\n";
        auto start = std::chrono::steady_clock::now();

        for (const auto& test : tests_) {
            std::cout << "[ RUN      ] " << test.suite << "." << test.name << "\n";
            try {
                test.func();
                std::cout << "\033[32m[       OK ]\033[0m " << test.suite << "." << test.name << "\n";
                passed++;
            } catch (const std::exception& e) {
                std::cout << "\033[31m[  FAILED  ]\033[0m " << test.suite << "." << test.name 
                          << " | Error: " << e.what() << "\n";
                failed++;
            } catch (...) {
                std::cout << "\033[31m[  FAILED  ]\033[0m " << test.suite << "." << test.name 
                          << " | Unknown exception\n";
                failed++;
            }
        }

        auto end = std::chrono::steady_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "------------------------------------------------------------\n";
        std::cout << "Tests Total : " << tests_.size() << "\n"
                  << "Passed      : \033[32m" << passed << "\033[0m\n"
                  << "Failed      : " << (failed > 0 ? "\033[31m" : "\033[32m") << failed << "\033[0m\n"
                  << "Time Elapsed: " << elapsedMs << " ms\n"
                  << "============================================================\n\n";

        return (failed == 0) ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

struct TestRegistrar {
    TestRegistrar(const std::string& suite, const std::string& name, std::function<void()> func) {
        TestRegistry::getInstance().registerTest(suite, name, std::move(func));
    }
};

} // namespace netcore::testing

#define NETCORE_TEST(suite, name) \
    void suite##_##name##_Test(); \
    static ::netcore::testing::TestRegistrar registrar_##suite##_##name(#suite, #name, suite##_##name##_Test); \
    void suite##_##name##_Test()

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) throw std::runtime_error("Assertion failed: " #cond " at line " + std::to_string(__LINE__)); \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b " at line " + std::to_string(__LINE__)); \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) throw std::runtime_error("Assertion failed: " #a " != " #b " at line " + std::to_string(__LINE__)); \
    } while (0)

#define EXPECT_TRUE(cond) ASSERT_TRUE(cond)
#define EXPECT_FALSE(cond) ASSERT_FALSE(cond)
#define EXPECT_EQ(a, b) ASSERT_EQ(a, b)
#define EXPECT_NE(a, b) ASSERT_NE(a, b)
