// Ordo — 学习任务编排器
// 唯一入口：装配依赖、启动分发器或交互菜单（OUTLINE §7.1 第 3 条硬规则）。

#include "ordo/app/context.h"
#include "ordo/cli/commands/plan_cmd.h"
#include "ordo/cli/commands/stats_cmd.h"
#include "ordo/cli/commands/task_cmd.h"
#include "ordo/cli/dispatcher.h"
#include "ordo/cli/menu.h"
#include "ordo/core/types.h"
#include "ordo/io/config.h"
#include "ordo/io/event_log.h"
#include "ordo/io/file_repository.h"
#include "ordo/version.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

constexpr std::string_view kDefaultDataPath = "data/tasks.csv";

void enable_utf8_console() {
#if defined(_WIN32)
    // 源码与数据统一 UTF-8；把控制台代码页也切到 UTF-8，中文提示才不会乱码。
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

// Windows 的 main(argc, argv) 会把 Unicode 命令行按 ANSI 代码页转成窄字符串，
// 中文参数因此变成乱码。这里重新从 GetCommandLineW 取参数并统一转成 UTF-8。
std::vector<std::string> utf8_arguments(int argc, char** argv) {
    std::vector<std::string> result;
#if defined(_WIN32)
    int wide_count = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    if (wide != nullptr) {
        result.reserve(static_cast<std::size_t>(wide_count));
        for (int i = 0; i < wide_count; ++i) {
            const std::wstring_view item{wide[i]};
            if (item.empty()) {
                result.emplace_back();
                continue;
            }
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, item.data(),
                                                  static_cast<int>(item.size()), nullptr, 0,
                                                  nullptr, nullptr);
            std::string converted(static_cast<std::size_t>(bytes), '\0');
            WideCharToMultiByte(CP_UTF8, 0, item.data(), static_cast<int>(item.size()),
                                converted.data(), bytes, nullptr, nullptr);
            result.push_back(std::move(converted));
        }
        LocalFree(wide);
    }
#endif
    if (result.empty()) {
        result.reserve(static_cast<std::size_t>(argc));
        for (int i = 0; i < argc; ++i) {
            result.emplace_back(argv[i]);
        }
    }
    return result;
}

bool all_digits(std::string_view text) {
    if (text.empty()) {
        return false;
    }
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

bool looks_like_date(std::string_view text) {
    return text.size() == 10 && text[4] == '-' && text[7] == '-' &&
           all_digits(text.substr(0, 4)) && all_digits(text.substr(5, 2)) &&
           all_digits(text.substr(8, 2));
}

bool looks_like_time(std::string_view text) {
    return text.size() == 5 && text[2] == ':' && all_digits(text.substr(0, 2)) &&
           all_digits(text.substr(3, 2));
}

struct Cli {
    ordo::Args args;
    std::filesystem::path data_path{std::string{kDefaultDataPath}};
    bool want_version{false};
    bool want_help{false};
    std::string error;
};

Cli parse_cli(const std::vector<std::string>& arguments) {
    Cli cli;

    for (std::size_t i = 1; i < arguments.size(); ++i) {
        const std::string token = arguments[i];

        if (token == "--version" || token == "-v") {
            cli.want_version = true;
            continue;
        }
        if (token == "--help" || token == "-h") {
            cli.want_help = true;
            continue;
        }
        if (token == "--data") {
            if (i + 1 >= arguments.size()) {
                cli.error = "--data 需要一个文件路径";
                return cli;
            }
            cli.data_path = arguments[++i];
            continue;
        }
        if (token.rfind("--data=", 0) == 0) {
            cli.data_path = token.substr(std::string{"--data="}.size());
            continue;
        }

        if (token.rfind("--", 0) == 0 && token.size() > 2) {
            const std::size_t equals = token.find('=');
            if (equals != std::string::npos) {
                const std::string name = token.substr(2, equals - 2);
                std::string value = token.substr(equals + 1);
                // --due=2026-10-30 23:59 同样允许不写引号。
                if (name == "due" && looks_like_date(value) &&
                    i + 1 < arguments.size() && looks_like_time(arguments[i + 1])) {
                    value += ' ';
                    value += arguments[++i];
                }
                cli.args.flags[name] = std::move(value);
                continue;
            }

            const std::string name = token.substr(2);
            if (i + 1 >= arguments.size() ||
                std::string_view{arguments[i + 1]}.rfind("--", 0) == 0) {
                cli.args.flags[name] = "";
                continue;
            }

            std::string value = arguments[++i];
            // 允许 `--due 2026-10-30 23:59` 不写引号：日期后紧跟 HH:MM 时合并为一个值。
            if (name == "due" && looks_like_date(value) && i + 1 < arguments.size() &&
                looks_like_time(arguments[i + 1])) {
                value += ' ';
                value += arguments[++i];
            }
            cli.args.flags[name] = std::move(value);
            continue;
        }

        cli.args.positional.push_back(token);
    }

    return cli;
}

}  // namespace

int main(int argc, char** argv) {
    enable_utf8_console();

    const Cli cli = parse_cli(utf8_arguments(argc, argv));
    if (!cli.error.empty()) {
        std::cerr << "错误: " << cli.error << '\n';
        return ordo::kExitUsage;
    }
    if (cli.want_version) {
        std::cout << "ordo " << ORDO_VERSION_STRING << '\n';
        return ordo::kExitOk;
    }

    try {
        // 唯一的依赖装配点：存储实现只在这里构造一次，向下以引用传递。
        ordo::CsvTaskRepository repo(cli.data_path);
        // 日志与数据文件同目录：--data 指向哪里，log.csv 就跟着去哪里。
        ordo::CsvEventLog event_log(cli.data_path.parent_path() / "log.csv");

        // 配置文件与数据文件同目录；文件不存在时使用内置默认值。
        const std::filesystem::path config_path = cli.data_path.parent_path() / "config.ini";
        auto schedule = ordo::load_config(config_path);
        if (!schedule) {
            std::cerr << "错误: " << schedule.status().message << '\n';
            return ordo::kExitError;
        }

        ordo::AppContext ctx{repo, &event_log, std::cout, std::cin,
                             [] { return ordo::Clock::now(); }, schedule.value()};

        ordo::Dispatcher dispatcher;
        ordo::register_task_commands(dispatcher);
        ordo::register_plan_commands(dispatcher);
        ordo::register_stats_commands(dispatcher);

        if (cli.want_help) {
            std::cout << "Ordo " << ORDO_VERSION_STRING << " - 学习任务编排器\n\n";
            dispatcher.print_help(std::cout);
            std::cout << "\n全局选项:\n"
                      << "  --data <路径>   指定数据文件（默认 " << kDefaultDataPath << "）\n";
            return ordo::kExitOk;
        }

        if (cli.args.positional.empty()) {
            ordo::Menu menu(dispatcher);
            return menu.run(ctx);
        }

        return dispatcher.run(cli.args, ctx);
    } catch (const std::exception& error) {
        std::cerr << "内部错误: " << error.what() << '\n';
        return ordo::kExitError;
    } catch (...) {
        std::cerr << "内部错误: 未知异常\n";
        return ordo::kExitError;
    }
}
