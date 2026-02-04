#pragma once

#include "xswl/signals.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <stdexcept>

// 测试宏与断言
#define TEST_CASE(name) \
    void test_##name(); \
    struct TestRegister_##name { \
        TestRegister_##name() { \
            TestRunner::instance().add(#name, test_##name); \
        } \
    } g_test_register_##name; \
    void test_##name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    } while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b)    ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b)    ASSERT_TRUE((a) != (b))
#define ASSERT_GT(a, b)    ASSERT_TRUE((a) > (b))
#define ASSERT_GE(a, b)    ASSERT_TRUE((a) >= (b))
#define ASSERT_LT(a, b)    ASSERT_TRUE((a) < (b))
#define ASSERT_LE(a, b)    ASSERT_TRUE((a) <= (b))

// 测试运行器
class TestRunner
{
public:
    typedef void (*TestFunc)();

    static TestRunner& instance()
    {
        static TestRunner runner;
        return runner;
    }

    void add(const std::string& name, TestFunc func)
    {
        tests_.push_back({name, func});
    }

    int run()
    {
        int passed = 0;
        int failed = 0;

        std::cout << "Running " << tests_.size() << " tests...\n";
        std::cout << std::string(60, '=') << "\n";

        for (const auto& test : tests_)
        {
            std::cout << "[ RUN      ] " << test.name << std::endl;
            try
            {
                test.func();
                std::cout << "[       OK ] " << test.name << std::endl;
                ++passed;
            }
            catch (const std::exception& e)
            {
                std::cout << "[  FAILED  ] " << test.name << "\n";
                std::cout << "             " << e.what() << std::endl;
                ++failed;
            }
        }

        std::cout << std::string(60, '=') << "\n";
        std::cout << passed << " passed, " << failed << " failed\n";

        return failed;
    }

private:
    struct TestCase
    {
        std::string name;
        TestFunc func;
    };

    std::vector<TestCase> tests_;
};

// 常用测试类型
class Counter
{
public:
    Counter() : count_(0) {}

    void increment() { ++count_; }
    void increment_by(int n) { count_ += n; }
    void set(int v) { count_ = v; }
    int get() const { return count_; }
    void reset() { count_ = 0; }

private:
    std::atomic<int> count_;
};

class Receiver : public std::enable_shared_from_this<Receiver>
{
public:
    Receiver() : call_count_(0), last_value_(0) {}

    void on_signal() { ++call_count_; }
    void on_value(int v) { ++call_count_; last_value_ = v; }
    void on_two_values(int a, int b) { ++call_count_; last_value_ = a + b; }
    void on_string(const std::string& s) { ++call_count_; last_string_ = s; }

    int call_count() const { return call_count_; }
    int last_value() const { return last_value_; }
    const std::string& last_string() const { return last_string_; }

    void reset()
    {
        call_count_ = 0;
        last_value_ = 0;
        last_string_.clear();
    }

private:
    std::atomic<int> call_count_;
    std::atomic<int> last_value_;
    std::string last_string_;
};
