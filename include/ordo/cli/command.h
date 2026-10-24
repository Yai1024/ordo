#pragma once

// C1 命令层基类（OUTLINE §7.6）。
// 命令只做三件事：读入参 → 调服务 → 打印结果。不得出现文件 I/O 与算法实现。

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ordo/app/context.h"

namespace ordo {

// 退出码约定：0 成功；1 业务失败（数据未变）；2 用法错误。
constexpr int kExitOk = 0;
constexpr int kExitError = 1;
constexpr int kExitUsage = 2;

struct Args {
    std::vector<std::string> positional;
    std::map<std::string, std::string> flags;

    bool has_flag(std::string_view name) const;
    std::optional<std::string> flag(std::string_view name) const;
    std::string flag_or(std::string_view name, std::string fallback) const;
};

class Command {
public:
    virtual ~Command() = default;

    // 点分两级：task.add / task.list / ...
    virtual std::string_view name() const noexcept = 0;
    virtual std::string_view help() const noexcept = 0;

    // AppContext 让命令拿到仓库、输出流与时钟；返回值即进程退出码。
    virtual int execute(const Args& args, AppContext& ctx) = 0;
};

}  // namespace ordo
