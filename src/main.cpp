// Ordo — 学习任务编排器
// M0 骨架：只验证工具链与仓库结构，业务功能从 M1 开始。
// 本文件是唯一的依赖装配点（见 docs/OUTLINE.md §7.1 第 3 条硬规则）。

#include <iostream>
#include <string>
#include <string_view>

#include "ordo/version.h"

namespace {

void print_usage(std::ostream& out) {
    out << "Ordo " << ORDO_VERSION_STRING << " - study task orchestrator\n\n"
        << "usage: ordo [command] [options]\n\n"
        << "  --version    print version and exit\n"
        << "  --help       print this help and exit\n\n"
        << "No command given: the interactive menu will start (available from M1).\n"
        << "Design document: docs/OUTLINE.md\n";
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--version" || arg == "-v") {
            std::cout << "ordo " << ORDO_VERSION_STRING << '\n';
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            print_usage(std::cout);
            return 0;
        }
        std::cerr << "unknown option: " << arg << "\n";
        print_usage(std::cerr);
        return 2;
    }

    // M0：骨架就绪。M1 将在此装配 CsvTaskRepository 与 Dispatcher。
    std::cout << "ordo " << ORDO_VERSION_STRING << " - skeleton build (M0)\n";
    return 0;
}
