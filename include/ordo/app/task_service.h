#pragma once

// C2 应用服务：任务的增删改查与统计（OUTLINE §7.5）。
// 唯一同时知道"数据从哪来"和"算法怎么用"的层；不做格式转换、不打印、不解析参数。

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "ordo/core/result.h"
#include "ordo/core/task.h"
#include "ordo/io/event_log.h"
#include "ordo/io/repository.h"

namespace ordo {

enum class RankMode { None, Score, Edf };

struct Stats {
    std::size_t total{0};
    std::size_t todo{0};
    std::size_t doing{0};
    std::size_t done{0};
    std::size_t dropped{0};
    std::size_t overdue{0};      // 未完成且截止时间已过
    Minutes open_estimate{0};    // 未完成任务的预估总时长
    Minutes done_estimate{0};    // 已完成任务的预估总时长
};

// 周统计的一行：某类型投入了多少分钟（ADR-0004）。
struct KindMinutes {
    TaskKind kind{TaskKind::Other};
    Minutes minutes{0};
};

// 最近 7 个自然日的完成率与投入分布；全部来自 log.csv。
struct WeeklyStats {
    TimePoint window_begin{};
    TimePoint window_end{};
    std::size_t completed{0};   // 窗口内完成的不同任务数
    std::size_t expected{0};    // 完成 + 窗口内到期仍未完成（并集）
    bool has_rate{false};       // expected 为 0 时为 false
    int completion_percent{0};
    Minutes total_minutes{0};
    std::vector<KindMinutes> by_kind;  // 按分钟数降序，只含有投入的类型
};

class TaskService {
public:
    // log 可空：测试与只读场景不需要日志；生产装配点传入 CsvEventLog。
    explicit TaskService(ITaskRepository& repo, IEventLog* log = nullptr);

    // 分配 id、校验、落盘。draft.id 为 0 时自动分配；非 0 时重复即拒绝。
    Result<Task> add(Task draft);

    // patch 必须是完整记录（id 与参数一致）；字段级合并由调用方完成后传入。
    Status update(TaskId id, const Task& patch);

    // 专门的改状态入口：完成时写 task.done 日志，并记录可选的实际投入。
    Status set_state(TaskId id, TaskState state,
                     std::optional<Minutes> spent = std::nullopt);

    Status remove(TaskId id);
    Result<Task> get(TaskId id) const;

    Result<std::vector<Task>> list(std::optional<TaskState> state) const;

    // 排序入口：排完再返回完整记录，命令层不需要知道 TaskId → Task 的映射。
    Result<std::vector<Task>> list_ranked(std::optional<TaskState> state, RankMode mode,
                                          TimePoint now) const;

    Result<Stats> stats(TimePoint now) const;

    // M3：SM-2 评分、到期列表与周统计（ADR-0004）。
    Result<Task> grade_review(TaskId id, int quality, TimePoint now,
                              std::optional<Minutes> spent = std::nullopt);
    Result<std::vector<Task>> due_reviews(TimePoint now) const;
    Result<WeeklyStats> weekly_stats(TimePoint now) const;

private:
    void log_event(TimePoint when, std::string_view command, TaskId id,
                   std::optional<Minutes> minutes, std::string_view note) const;

    ITaskRepository& repo_;
    IEventLog* log_{nullptr};
};

}  // namespace ordo
