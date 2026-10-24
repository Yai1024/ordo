#pragma once

// 极简测试框架：零第三方依赖。
// 用法见 tests/test_version.cpp。
//   1. 用 ORDO_TEST(用例名) { ... } 定义一个用例；
//   2. 用例内用 ORDO_CHECK / ORDO_CHECK_EQ 断言；
//   3. make test 编译并运行，任一断言失败时进程返回非零。

#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ordo::test {

struct TestCase {
    std::string name;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

inline int& failure_count() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(std::string name, void (*fn)()) { registry().push_back({std::move(name), fn}); }
};

inline void report_failure(const char* file, int line, const std::string& what) {
    ++failure_count();
    std::fprintf(stderr, "  FAIL  %s:%d\n        %s\n", file, line, what.c_str());
}

}  // namespace ordo::test

#define ORDO_TEST(case_name)                                                       \
    static void case_name();                                                       \
    static ::ordo::test::Registrar case_name##_registrar{#case_name, case_name};   \
    static void case_name()

#define ORDO_CHECK(cond)                                                           \
    do {                                                                           \
        if (!(cond)) {                                                             \
            ::ordo::test::report_failure(__FILE__, __LINE__, "assert: " #cond);    \
        }                                                                          \
    } while (0)

#define ORDO_CHECK_EQ(actual, expected)                                            \
    do {                                                                           \
        const auto& a_ = (actual);                                                 \
        const auto& e_ = (expected);                                               \
        if (!(a_ == e_)) {                                                         \
            std::ostringstream os_;                                                \
            os_ << #actual " == " #expected << "\n        actual  : " << a_        \
                << "\n        expected: " << e_;                                   \
            ::ordo::test::report_failure(__FILE__, __LINE__, os_.str());           \
        }                                                                          \
    } while (0)
