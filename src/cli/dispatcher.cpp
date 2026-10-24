#include "ordo/cli/dispatcher.h"

#include <algorithm>
#include <iomanip>
#include <utility>
#include <vector>

namespace ordo {

void Dispatcher::add(std::unique_ptr<Command> command) {
    if (!command) {
        return;
    }
    const std::string name{command->name()};
    commands_[name] = std::move(command);
}

int Dispatcher::run(const Args& args, AppContext& ctx) {
    if (args.positional.empty()) {
        print_help(ctx.out);
        return kExitUsage;
    }

    std::string name = args.positional[0];
    std::size_t consumed = 1;

    if (args.positional.size() >= 2) {
        const std::string two_level = name + "." + args.positional[1];
        if (commands_.find(two_level) != commands_.end()) {
            name = two_level;
            consumed = 2;
        }
    }

    const auto found = commands_.find(name);
    if (found == commands_.end()) {
        ctx.out << "未知命令: " << name << "\n\n";
        print_help(ctx.out);
        return kExitUsage;
    }

    Args rest;
    rest.positional.assign(args.positional.begin() + static_cast<std::ptrdiff_t>(consumed),
                           args.positional.end());
    rest.flags = args.flags;

    return found->second->execute(rest, ctx);
}

int Dispatcher::execute(std::string_view name, const Args& args, AppContext& ctx) {
    const auto found = commands_.find(std::string{name});
    if (found == commands_.end()) {
        ctx.out << "未知命令: " << name << '\n';
        return kExitUsage;
    }
    return found->second->execute(args, ctx);
}

void Dispatcher::print_help(std::ostream& out) const {
    out << "用法:\n"
        << "  ordo                          进入交互菜单\n"
        << "  ordo <命令> [参数] [选项]      执行一条命令\n"
        << "  ordo --version | --help       查看版本或本帮助\n\n"
        << "可用命令:\n";

    std::vector<const Command*> ordered;
    ordered.reserve(commands_.size());
    for (const auto& entry : commands_) {
        ordered.push_back(entry.second.get());
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const Command* left, const Command* right) {
                  return left->name() < right->name();
              });

    for (const Command* command : ordered) {
        out << "  " << std::left << std::setw(14) << command->name() << command->help() << '\n';
    }
}

}  // namespace ordo
