#include "ordo/cli/commands/task_cmd.h"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "ordo/app/task_service.h"
#include "ordo/core/task.h"
#include "ordo/core/types.h"

namespace ordo {
namespace {

std::optional<int> parse_int_value(std::string_view text) {
    const std::string_view cleaned = trim(text);
    if (cleaned.empty()) {
        return std::nullopt;
    }
    int value = 0;
    const char* first = cleaned.data();
    const char* last = cleaned.data() + cleaned.size();
    const std::from_chars_result result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

std::optional<TaskId> parse_id_value(std::string_view text) {
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

std::optional<TaskId> task_id_from(const Args& args) {
    if (args.positional.empty()) {
        return std::nullopt;
    }
    return parse_id_value(args.positional.front());
}

int usage_error(AppContext& ctx, std::string_view message, std::string_view help) {
    ctx.out << "错误: " << message << "\n用法: " << help << '\n';
    return kExitUsage;
}

int report_error(AppContext& ctx, const Status& status) {
    ctx.out << "错误: " << status.message << '\n';
    return kExitError;
}

void print_table_header(std::ostream& out) {
    out << std::left << std::setw(4) << "ID" << std::setw(8) << "STATE" << std::setw(4) << "IMP"
        << std::setw(5) << "EST" << std::setw(18) << "DEADLINE" << "TITLE\n";
}

void print_task_row(std::ostream& out, const Task& task) {
    out << std::left << std::setw(4) << task.id << std::setw(8) << to_string(task.state)
        << std::setw(4) << task.importance << std::setw(5) << task.estimate << std::setw(18)
        << format_datetime(task.deadline) << task.title;
    if (!task.course.empty()) {
        out << " [" << task.course << "]";
    }
    out << '\n';
}

// ---- task.add ----

class TaskAddCommand final : public Command {
public:
    std::string_view name() const noexcept override { return "task.add"; }

    std::string_view help() const noexcept override {
        return "添加任务: task add <标题> --due \"YYYY-MM-DD HH:MM\" "
               "[--course 课程] [--importance 1-5] [--est 分钟]";
    }

    int execute(const Args& args, AppContext& ctx) override {
        std::string title;
        for (const std::string& part : args.positional) {
            if (!title.empty()) {
                title.push_back(' ');
            }
            title += part;
        }
        if (trim(title).empty()) {
            title = args.flag_or("title", "");
        }
        if (trim(title).empty()) {
            return usage_error(ctx, "缺少任务标题", help());
        }

        const std::optional<std::string> due_text = args.flag("due");
        if (!due_text.has_value()) {
            return usage_error(ctx, "缺少 --due 截止时间", help());
        }
        const std::optional<TimePoint> deadline = parse_datetime(*due_text);
        if (!deadline.has_value()) {
            return usage_error(ctx, "截止时间格式应为 YYYY-MM-DD HH:MM", help());
        }

        int importance = kDefaultImportance;
        if (const std::optional<std::string> value = args.flag("importance")) {
            const std::optional<int> parsed = parse_int_value(*value);
            if (!parsed.has_value()) {
                return usage_error(ctx, "--importance 必须是整数", help());
            }
            importance = *parsed;
        }

        Minutes estimate = 0;
        if (const std::optional<std::string> value = args.flag("est")) {
            const std::optional<int> parsed = parse_int_value(*value);
            if (!parsed.has_value()) {
                return usage_error(ctx, "--est 必须是整数分钟", help());
            }
            estimate = static_cast<Minutes>(*parsed);
        }

        Task draft;
        draft.title = std::string{trim(title)};
        draft.course = args.flag_or("course", "");
        draft.importance = importance;
        draft.estimate = estimate;
        draft.deadline = *deadline;

        TaskService service(ctx.repo);
        auto result = service.add(std::move(draft));
        if (!result) {
            return report_error(ctx, result.status());
        }

        const Task& added = result.value();
        ctx.out << "已添加任务 #" << added.id << "：" << added.title << "（截止 "
                << format_datetime(added.deadline) << "）\n";
        return kExitOk;
    }
};

// ---- task.list ----

class TaskListCommand final : public Command {
public:
    std::string_view name() const noexcept override { return "task.list"; }

    std::string_view help() const noexcept override {
        return "列出任务: task list [--state open|todo|doing|done|dropped|all] "
               "[--rank none|score|edf]";
    }

    int execute(const Args& args, AppContext& ctx) override {
        std::optional<TaskState> state_filter;
        bool open_only = true;  // 默认只看仍需投入时间的任务

        if (const std::optional<std::string> value = args.flag("state")) {
            if (*value == "open") {
                open_only = true;
            } else if (*value == "all") {
                open_only = false;
            } else {
                const std::optional<TaskState> parsed = parse_task_state(*value);
                if (!parsed.has_value()) {
                    return usage_error(ctx, "--state 只能是 open/todo/doing/done/dropped/all",
                                       help());
                }
                state_filter = *parsed;
                open_only = false;
            }
        }

        RankMode mode = RankMode::None;
        if (const std::optional<std::string> value = args.flag("rank")) {
            if (*value == "none") {
                mode = RankMode::None;
            } else if (*value == "score") {
                mode = RankMode::Score;
            } else if (*value == "edf") {
                mode = RankMode::Edf;
            } else {
                return usage_error(ctx, "--rank 只能是 none/score/edf", help());
            }
        }

        TaskService service(ctx.repo);
        auto result = service.list_ranked(state_filter, mode, ctx.now());
        if (!result) {
            return report_error(ctx, result.status());
        }

        std::vector<Task> tasks = std::move(result.value());
        if (open_only) {
            // "open" 是两个状态的集合，TaskService::list 只接受单个状态，
            // 因此先取全量、排好序，再在这里做展示过滤；相对顺序不受影响。
            tasks.erase(std::remove_if(tasks.begin(), tasks.end(),
                                       [](const Task& task) { return !is_open(task.state); }),
                        tasks.end());
        }

        if (tasks.empty()) {
            ctx.out << "（没有符合条件的任务）\n";
            return kExitOk;
        }

        print_table_header(ctx.out);
        for (const Task& task : tasks) {
            print_task_row(ctx.out, task);
        }
        ctx.out << "共 " << tasks.size() << " 条\n";
        return kExitOk;
    }
};

// ---- task.done ----

class TaskDoneCommand final : public Command {
public:
    std::string_view name() const noexcept override { return "task.done"; }
    std::string_view help() const noexcept override { return "标记完成: task done <id>"; }

    int execute(const Args& args, AppContext& ctx) override {
        const std::optional<TaskId> id = task_id_from(args);
        if (!id.has_value()) {
            return usage_error(ctx, "缺少或非法任务 id", help());
        }

        TaskService service(ctx.repo);
        auto found = service.get(*id);
        if (!found) {
            return report_error(ctx, found.status());
        }

        Task task = std::move(found.value());
        if (task.state == TaskState::Done) {
            ctx.out << "任务 #" << task.id << " 已经是完成状态\n";
            return kExitOk;
        }

        task.state = TaskState::Done;
        const Status updated = service.update(task.id, task);
        if (!updated.ok()) {
            return report_error(ctx, updated);
        }

        ctx.out << "任务 #" << task.id << " 已标记完成：" << task.title << '\n';
        return kExitOk;
    }
};

// ---- task.edit ----

class TaskEditCommand final : public Command {
public:
    std::string_view name() const noexcept override { return "task.edit"; }

    std::string_view help() const noexcept override {
        return "修改任务: task edit <id> [--title ...] [--course ...] [--importance 1-5] "
               "[--est 分钟] [--due ...] [--state ...]";
    }

    int execute(const Args& args, AppContext& ctx) override {
        const std::optional<TaskId> id = task_id_from(args);
        if (!id.has_value()) {
            return usage_error(ctx, "缺少或非法任务 id", help());
        }

        TaskService service(ctx.repo);
        auto found = service.get(*id);
        if (!found) {
            return report_error(ctx, found.status());
        }

        Task task = std::move(found.value());
        bool changed = false;

        if (const std::optional<std::string> value = args.flag("title")) {
            const std::string_view cleaned = trim(*value);
            if (cleaned.empty()) {
                return usage_error(ctx, "--title 不能为空", help());
            }
            task.title = std::string{cleaned};
            changed = true;
        }
        if (const std::optional<std::string> value = args.flag("course")) {
            task.course = *value;
            changed = true;
        }
        if (const std::optional<std::string> value = args.flag("importance")) {
            const std::optional<int> parsed = parse_int_value(*value);
            if (!parsed.has_value()) {
                return usage_error(ctx, "--importance 必须是整数", help());
            }
            task.importance = *parsed;
            changed = true;
        }
        if (const std::optional<std::string> value = args.flag("est")) {
            const std::optional<int> parsed = parse_int_value(*value);
            if (!parsed.has_value()) {
                return usage_error(ctx, "--est 必须是整数分钟", help());
            }
            task.estimate = static_cast<Minutes>(*parsed);
            changed = true;
        }
        if (const std::optional<std::string> value = args.flag("due")) {
            const std::optional<TimePoint> parsed = parse_datetime(*value);
            if (!parsed.has_value()) {
                return usage_error(ctx, "截止时间格式应为 YYYY-MM-DD HH:MM", help());
            }
            task.deadline = *parsed;
            changed = true;
        }
        if (const std::optional<std::string> value = args.flag("state")) {
            const std::optional<TaskState> parsed = parse_task_state(*value);
            if (!parsed.has_value()) {
                return usage_error(ctx, "--state 只能是 todo/doing/done/dropped", help());
            }
            task.state = *parsed;
            changed = true;
        }

        if (!changed) {
            return usage_error(ctx, "没有指定要修改的字段", help());
        }

        const Status updated = service.update(task.id, task);
        if (!updated.ok()) {
            return report_error(ctx, updated);
        }

        ctx.out << "已更新任务 #" << task.id << "：" << task.title << '\n';
        return kExitOk;
    }
};

// ---- task.remove ----

class TaskRemoveCommand final : public Command {
public:
    std::string_view name() const noexcept override { return "task.remove"; }
    std::string_view help() const noexcept override { return "删除任务: task remove <id>"; }

    int execute(const Args& args, AppContext& ctx) override {
        const std::optional<TaskId> id = task_id_from(args);
        if (!id.has_value()) {
            return usage_error(ctx, "缺少或非法任务 id", help());
        }

        TaskService service(ctx.repo);
        const Status removed = service.remove(*id);
        if (!removed.ok()) {
            return report_error(ctx, removed);
        }

        ctx.out << "已删除任务 #" << *id << '\n';
        return kExitOk;
    }
};

// ---- task.stats ----

class TaskStatsCommand final : public Command {
public:
    std::string_view name() const noexcept override { return "task.stats"; }
    std::string_view help() const noexcept override { return "统计概览: task stats"; }

    int execute(const Args& /*args*/, AppContext& ctx) override {
        TaskService service(ctx.repo);
        auto result = service.stats(ctx.now());
        if (!result) {
            return report_error(ctx, result.status());
        }

        const Stats& stats = result.value();
        ctx.out << "任务总数 " << stats.total << "：待办 " << stats.todo << " / 进行中 "
                << stats.doing << " / 已完成 " << stats.done << " / 已放弃 " << stats.dropped
                << '\n'
                << "超期未完成 " << stats.overdue << " 条\n"
                << "未完成预估总时长 " << stats.open_estimate << " 分钟（已完成 "
                << stats.done_estimate << " 分钟）\n";
        return kExitOk;
    }
};

}  // namespace

void register_task_commands(Dispatcher& dispatcher) {
    dispatcher.add(std::make_unique<TaskAddCommand>());
    dispatcher.add(std::make_unique<TaskListCommand>());
    dispatcher.add(std::make_unique<TaskDoneCommand>());
    dispatcher.add(std::make_unique<TaskEditCommand>());
    dispatcher.add(std::make_unique<TaskRemoveCommand>());
    dispatcher.add(std::make_unique<TaskStatsCommand>());
}

}  // namespace ordo
