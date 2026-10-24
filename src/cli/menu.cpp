#include "ordo/cli/menu.h"

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "ordo/app/task_service.h"
#include "ordo/core/task.h"
#include "ordo/core/types.h"

namespace ordo {
namespace {

std::optional<std::string> prompt(AppContext& ctx, std::string_view label) {
    ctx.out << label;
    ctx.out.flush();
    std::string line;
    if (!std::getline(ctx.in, line)) {
        return std::nullopt;
    }
    return line;
}

std::string use_default(const std::string& input, std::string fallback) {
    const std::string_view cleaned = trim(input);
    return cleaned.empty() ? std::move(fallback) : std::string{cleaned};
}

std::optional<TaskId> parse_id(std::string_view text) {
    const std::string_view cleaned = trim(text);
    if (cleaned.empty()) {
        return std::nullopt;
    }
    TaskId value = 0;
    const char* first = cleaned.data();
    const char* last = cleaned.data() + cleaned.size();
    const std::from_chars_result result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last || value == 0) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

Menu::Menu(Dispatcher& dispatcher) : dispatcher_(dispatcher) {}

int Menu::run(AppContext& ctx) {
    while (true) {
        ctx.out << "\n== Ordo 学习任务编排器 ==\n"
                << "  1. 添加任务\n"
                << "  2. 查看任务\n"
                << "  3. 标记完成\n"
                << "  4. 修改任务\n"
                << "  5. 删除任务\n"
                << "  6. 统计概览\n"
                << "  0. 退出\n"
                << "请选择: ";

        std::string line;
        if (!std::getline(ctx.in, line)) {
            ctx.out << '\n';
            return kExitOk;
        }

        const std::string_view choice = trim(line);
        bool keep_running = true;

        if (choice == "0" || choice == "q" || choice == "quit") {
            return kExitOk;
        } else if (choice == "1") {
            keep_running = handle_add(ctx);
        } else if (choice == "2") {
            keep_running = handle_list(ctx);
        } else if (choice == "3") {
            keep_running = handle_done(ctx);
        } else if (choice == "4") {
            keep_running = handle_edit(ctx);
        } else if (choice == "5") {
            keep_running = handle_remove(ctx);
        } else if (choice == "6") {
            keep_running = handle_stats(ctx);
        } else {
            ctx.out << "无效选项，请输入 0-6\n";
        }

        if (!keep_running) {
            ctx.out << '\n';
            return kExitOk;
        }
    }
}

bool Menu::handle_add(AppContext& ctx) {
    const std::optional<std::string> title = prompt(ctx, "标题: ");
    if (!title.has_value()) {
        return false;
    }
    const std::optional<std::string> course = prompt(ctx, "课程（可空）: ");
    if (!course.has_value()) {
        return false;
    }
    const std::optional<std::string> importance = prompt(ctx, "重要度 1-5 [3]: ");
    if (!importance.has_value()) {
        return false;
    }
    const std::optional<std::string> estimate = prompt(ctx, "预估分钟 [60]: ");
    if (!estimate.has_value()) {
        return false;
    }
    const std::optional<std::string> due = prompt(ctx, "截止时间 YYYY-MM-DD HH:MM: ");
    if (!due.has_value()) {
        return false;
    }

    Args args;
    args.positional.push_back(use_default(*title, ""));
    args.flags["course"] = use_default(*course, "");
    args.flags["importance"] = use_default(*importance, "3");
    args.flags["est"] = use_default(*estimate, "60");
    args.flags["due"] = use_default(*due, "");

    dispatcher_.execute("task.add", args, ctx);
    return true;
}

bool Menu::handle_list(AppContext& ctx) {
    dispatcher_.execute("task.list", Args{}, ctx);
    return true;
}

bool Menu::handle_done(AppContext& ctx) {
    const std::optional<std::string> id_text = prompt(ctx, "任务 id: ");
    if (!id_text.has_value()) {
        return false;
    }

    Args args;
    args.positional.push_back(use_default(*id_text, ""));
    dispatcher_.execute("task.done", args, ctx);
    return true;
}

bool Menu::handle_edit(AppContext& ctx) {
    const std::optional<std::string> id_text = prompt(ctx, "任务 id: ");
    if (!id_text.has_value()) {
        return false;
    }

    const std::optional<TaskId> id = parse_id(*id_text);
    if (!id.has_value()) {
        ctx.out << "错误: 任务 id 非法\n";
        return true;
    }

    TaskService service(ctx.repo);
    auto found = service.get(*id);
    if (!found) {
        ctx.out << "错误: " << found.status().message << '\n';
        return true;
    }

    const Task current = std::move(found.value());
    ctx.out << "当前: #" << current.id << " " << current.title << "（重要度 "
            << current.importance << "，预估 " << current.estimate << " 分钟，截止 "
            << format_datetime(current.deadline) << "，状态 " << to_string(current.state) << "）\n";

    const std::optional<std::string> title =
        prompt(ctx, "标题 [" + current.title + "]: ");
    if (!title.has_value()) {
        return false;
    }
    const std::optional<std::string> course = prompt(ctx, "课程 [" + current.course + "]: ");
    if (!course.has_value()) {
        return false;
    }
    const std::optional<std::string> importance =
        prompt(ctx, "重要度 [" + std::to_string(current.importance) + "]: ");
    if (!importance.has_value()) {
        return false;
    }
    const std::optional<std::string> estimate =
        prompt(ctx, "预估分钟 [" + std::to_string(current.estimate) + "]: ");
    if (!estimate.has_value()) {
        return false;
    }
    const std::optional<std::string> due =
        prompt(ctx, "截止时间 [" + format_datetime(current.deadline) + "]: ");
    if (!due.has_value()) {
        return false;
    }
    const std::optional<std::string> state =
        prompt(ctx, "状态 [todo/doing/done/dropped] [" + std::string{to_string(current.state)} +
                        "]: ");
    if (!state.has_value()) {
        return false;
    }

    Args args;
    args.positional.push_back(std::to_string(current.id));
    args.flags["title"] = use_default(*title, current.title);
    args.flags["course"] = use_default(*course, current.course);
    args.flags["importance"] = use_default(*importance, std::to_string(current.importance));
    args.flags["est"] = use_default(*estimate, std::to_string(current.estimate));
    args.flags["due"] = use_default(*due, format_datetime(current.deadline));
    args.flags["state"] = use_default(*state, std::string{to_string(current.state)});

    dispatcher_.execute("task.edit", args, ctx);
    return true;
}

bool Menu::handle_remove(AppContext& ctx) {
    const std::optional<std::string> id_text = prompt(ctx, "任务 id: ");
    if (!id_text.has_value()) {
        return false;
    }

    const std::optional<std::string> confirm = prompt(ctx, "确认删除? [y/N]: ");
    if (!confirm.has_value()) {
        return false;
    }

    const std::string_view answer = trim(*confirm);
    if (answer != "y" && answer != "Y" && answer != "yes") {
        ctx.out << "已取消\n";
        return true;
    }

    Args args;
    args.positional.push_back(use_default(*id_text, ""));
    dispatcher_.execute("task.remove", args, ctx);
    return true;
}

bool Menu::handle_stats(AppContext& ctx) {
    dispatcher_.execute("task.stats", Args{}, ctx);
    return true;
}

}  // namespace ordo
