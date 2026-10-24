// 测试入口：注册表在静态初始化阶段收集全部用例，这里逐个运行并汇总。

#include <cstdio>

#include "test_framework.h"

int main() {
    const auto& cases = ordo::test::registry();

    std::printf("running %zu test case(s)\n\n", cases.size());
    for (const auto& c : cases) {
        const int before = ordo::test::failure_count();
        c.fn();
        const bool passed = ordo::test::failure_count() == before;
        std::printf("  %s  %s\n", passed ? "ok  " : "FAIL", c.name.c_str());
    }

    const int failures = ordo::test::failure_count();
    std::printf("\n%zu case(s), %d assertion failure(s)\n", cases.size(), failures);
    return failures == 0 ? 0 : 1;
}
