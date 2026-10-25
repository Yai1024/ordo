#include "ordo/cli/commands/stats_cmd.h"

#include <iomanip>
#include <memory>
#include <string>
#include <string_view>

#include "ordo/app/task_service.h"
#include "ordo/core/task.h"
#include "ordo/core/types.h"

namespace ordo {
namespace {

int report_error(AppContext& ctx, const Status& status) {
    ctx.out << "错误: " << status.message << '\n';
    return kExitError;
}

std::string percent_text(int percent) { return std::to_string(percent) + "%"; }

// ---- stats.week ----

class StatsWeekCommand final : public Command {
public:
    std::string_view name() const noexcept override { return "stats.week"; }
    std::string_view help() const noexcept override {
        return "本周统计: stats week（最近 7 天完成率与投入分布）";
    }

    int execute(const Args& /*args*/, AppContext& ctx) override {
        TaskService service(ctx.repo, ctx.log);
        auto result = service.weekly_stats(ctx.now());
        if (!result) {
            return report_error(ctx, result.status());
        }

        const WeeklyStats& stats = result.value();
        ctx.out << "本周统计（" << format_date(stats.window_begin) << " ~ "
                << format_date(stats.window_end) << "）\n";

        if (stats.has_rate) {
            ctx.out << "完成 " << stats.completed << " / 应完成 " << stats.expected << "（"
                    << percent_text(stats.completion_percent) << "）\n";
        } else {
            ctx.out << "完成率 --（本周没有到期或完成的任务）\n";
        }

        if (stats.by_kind.empty()) {
            ctx.out << "投入 --（本周还没有记录投入）\n";
            ctx.out << "提示：用 task done <id> --spent 分钟 或 task review <id> --quality Q "
                       "--spent 分钟 记录实际投入。\n";
            return kExitOk;
        }

        ctx.out << "投入 " << stats.total_minutes << " 分钟\n";
        for (const KindMinutes& item : stats.by_kind) {
            const int percent = stats.total_minutes > 0
                                    ? static_cast<int>(item.minutes * 100 / stats.total_minutes)
                                    : 0;
            ctx.out << "  " << std::left << std::setw(10)
                    << std::string{kind_label(item.kind)} << std::right << std::setw(5)
                    << item.minutes << " 分钟（" << percent_text(percent) << "）\n";
        }
        return kExitOk;
    }
};

}  // namespace

void register_stats_commands(Dispatcher& dispatcher) {
    dispatcher.add(std::make_unique<StatsWeekCommand>());
}

}  // namespace ordo
