#include "ordo/cli/commands/plan_cmd.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ordo/app/plan_service.h"
#include "ordo/app/task_service.h"
#include "ordo/core/result.h"
#include "ordo/core/schedule.h"
#include "ordo/core/task.h"
#include "ordo/core/types.h"

namespace ordo {
namespace {

int usage_error(AppContext& ctx, std::string_view message, std::string_view help) {
    ctx.out << "错误: " << message << "\n用法: " << help << '\n';
    return kExitUsage;
}

int report_error(AppContext& ctx, const Status& status) {
    ctx.out << "错误: " << status.message << '\n';
    return kExitError;
}

std::optional<RankMode> parse_rank(const Args& args) {
    const std::optional<std::string> value = args.flag("rank");
    if (!value.has_value() || *value == "score") {
        return RankMode::Score;
    }
    if (*value == "edf") {
        return RankMode::Edf;
    }
    return std::nullopt;
}

std::string task_title(const std::unordered_map<TaskId, Task>& tasks, TaskId id) {
    const auto found = tasks.find(id);
    return found == tasks.end() ? std::string{"<未知任务>"} : found->second.title;
}

std::string task_kind_label(const std::unordered_map<TaskId, Task>& tasks, TaskId id) {
    const auto found = tasks.find(id);
    return found == tasks.end() ? std::string{"-"}
                                : std::string{kind_label(found->second.kind)};
}

void print_plan(AppContext& ctx, const Plan& plan,
                const std::unordered_map<TaskId, Task>& tasks, bool week, TimePoint now) {
    if (week) {
        ctx.out << "本周计划（" << format_date(now) << " 起 7 天）\n";
    } else {
        ctx.out << "今日计划（" << format_date(now) << "）\n";
    }

    if (plan.blocks.empty()) {
        ctx.out << "（没有可安排的时间块）\n";
    } else {
        for (const Block& block : plan.blocks) {
            if (week) {
                ctx.out << format_date(block.begin) << ' ';
            }
            ctx.out << format_time(block.begin) << '-' << format_time(block.end) << "  ["
                    << task_kind_label(tasks, block.task) << "] #" << block.task << ' '
                    << task_title(tasks, block.task) << '\n';
        }
    }

    ctx.out << "窗口 " << plan.total_free << " 分钟，已安排 " << plan.total_assigned
            << " 分钟\n";

    if (!plan.warnings.empty()) {
        ctx.out << "\n需要注意：\n";
        for (const Warning& warning : plan.warnings) {
            const auto found = tasks.find(warning.task);
            if (found != tasks.end() && !found->second.title.empty()) {
                ctx.out << "  #" << warning.task << ' ' << found->second.title << "："
                        << warning.reason << '\n';
            } else {
                ctx.out << "  #" << warning.task << "：" << warning.reason << '\n';
            }
        }
    }
}

int run_plan(const Args& args, AppContext& ctx, bool week) {
    const std::optional<RankMode> rank = parse_rank(args);
    if (!rank.has_value()) {
        return usage_error(ctx, "--rank 只能是 score/edf",
                           week ? "plan week [--rank score|edf]"
                                : "plan today [--rank score|edf]");
    }

    const TimePoint now = ctx.now();

    TaskService task_service(ctx.repo);
    auto all = task_service.list(std::nullopt);
    if (!all.has_value()) {
        return report_error(ctx, all.status());
    }

    std::unordered_map<TaskId, Task> tasks;
    tasks.reserve(all.value().size());
    for (const Task& task : all.value()) {
        tasks.emplace(task.id, task);
    }

    PlanService planner(ctx.repo, ctx.schedule);
    auto result = week ? planner.plan_week(now, *rank) : planner.plan_today(now, *rank);
    if (!result.has_value()) {
        return report_error(ctx, result.status());
    }

    print_plan(ctx, result.value(), tasks, week, now);
    return kExitOk;
}

}  // namespace

class PlanTodayCommand final : public Command {
public:
    std::string_view name() const noexcept override { return "plan.today"; }

    std::string_view help() const noexcept override {
        return "今日计划: plan today [--rank score|edf]";
    }

    int execute(const Args& args, AppContext& ctx) override { return run_plan(args, ctx, false); }
};

class PlanWeekCommand final : public Command {
public:
    std::string_view name() const noexcept override { return "plan.week"; }

    std::string_view help() const noexcept override {
        return "本周计划: plan week [--rank score|edf]";
    }

    int execute(const Args& args, AppContext& ctx) override { return run_plan(args, ctx, true); }
};

void register_plan_commands(Dispatcher& dispatcher) {
    dispatcher.add(std::make_unique<PlanTodayCommand>());
    dispatcher.add(std::make_unique<PlanWeekCommand>());
}

}  // namespace ordo
