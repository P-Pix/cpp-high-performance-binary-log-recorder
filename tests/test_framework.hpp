#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tests {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> func) {
        registry().push_back(TestCase{std::move(name), std::move(func)});
    }
};

class AssertionFailure : public std::runtime_error {
public:
    explicit AssertionFailure(const std::string& message) : std::runtime_error(message) {}
};

inline void fail(const char* file, int line, const std::string& message) {
    std::ostringstream oss;
    oss << file << ':' << line << ": " << message;
    throw AssertionFailure(oss.str());
}

template <typename A, typename B>
void require_eq(const A& actual, const B& expected, const char* actual_expr, const char* expected_expr, const char* file, int line) {
    if (!(actual == expected)) {
        std::ostringstream oss;
        oss << "expected " << actual_expr << " == " << expected_expr;
        fail(file, line, oss.str());
    }
}

} // namespace tests

#define HPBLR_TEST_UNIQUE2(a, b) a##b
#define HPBLR_TEST_UNIQUE(a, b) HPBLR_TEST_UNIQUE2(a, b)

#define TEST_CASE(name)                                                                            \
    static void HPBLR_TEST_UNIQUE(test_func_, __LINE__)();                                         \
    static ::tests::Registrar HPBLR_TEST_UNIQUE(test_registrar_, __LINE__)(name, HPBLR_TEST_UNIQUE(test_func_, __LINE__)); \
    static void HPBLR_TEST_UNIQUE(test_func_, __LINE__)()

#define REQUIRE_TRUE(expr)                                                                         \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            ::tests::fail(__FILE__, __LINE__, std::string("expected true: ") + #expr);             \
        }                                                                                          \
    } while (false)

#define REQUIRE_FALSE(expr) REQUIRE_TRUE(!(expr))

#define REQUIRE_EQ(actual, expected)                                                               \
    do {                                                                                           \
        ::tests::require_eq((actual), (expected), #actual, #expected, __FILE__, __LINE__);          \
    } while (false)

#define REQUIRE_THROWS_AS(expr, exception_type)                                                    \
    do {                                                                                           \
        bool caught_expected_exception = false;                                                     \
        try {                                                                                      \
            (void)(expr);                                                                          \
        } catch (const exception_type&) {                                                          \
            caught_expected_exception = true;                                                       \
        } catch (...) {                                                                            \
        }                                                                                          \
        if (!caught_expected_exception) {                                                          \
            ::tests::fail(__FILE__, __LINE__, std::string("expected exception: ") + #exception_type); \
        }                                                                                          \
    } while (false)
