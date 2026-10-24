#pragma once

// 命令注册表与分发（OUTLINE §7.6）。
// 同一套 Command 既服务 `ordo task add ...`，也服务交互菜单。

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ordo/cli/command.h"

namespace ordo {

class Dispatcher {
public:
    void add(std::unique_ptr<Command> command);

    // 从 positional 前两项解析命令名（"task" + "add" → "task.add"），
    // 未命中返回 kExitUsage。
    int run(const Args& args, AppContext& ctx);

    // 已解析出命令名时使用（菜单走这条路径）。
    int execute(std::string_view name, const Args& args, AppContext& ctx);

    void print_help(std::ostream& out) const;

    std::size_t size() const noexcept { return commands_.size(); }

private:
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
};

}  // namespace ordo
