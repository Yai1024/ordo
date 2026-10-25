#pragma once

// GUI 无关的界面模型（ADR-0002）。
// 这是未来任何外部 GUI（Qt / Web / ImGui）唯一的接入点：
//   - 只出现 std::string（UTF-8）、Result<T>、Status；
//   - 不出现 HWND、wchar_t、窗口消息、控件句柄；
//   - 业务规则仍由 app::TaskService 负责，本层只做表单解析与展示映射。

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ordo/app/task_service.h"
#include "ordo/core/result.h"
#include "ordo/core/task.h"
#include "ordo/core/types.h"
#include "ordo/io/config.h"
#include "ordo/io/event_log.h"
#include "ordo/io/repository.h"

namespace ordo {

struct ListQuery {
    std::optional<TaskState> state;  // 指定状态；空表示不按状态过滤
    bool open_only{true};            // state 为空时，是否只保留 todo/doing
    RankMode rank{RankMode::None};
};

// 列表一行。字符串已经格式化好，GUI 只负责显示。
struct TaskRow {
    TaskId id{};
    std::string title;
    std::string course;
    std::string kind_label;  // 课程作业 / 自主学习 / 活动 / 复习考试 / 其他
    int importance{0};
    Minutes estimate{0};
    std::string deadline_text;  // YYYY-MM-DD HH:MM
    TaskState state{TaskState::Todo};
    std::string review_text;    // 复习任务：未复习 / 到期 / N 天后；其余为空
};

// 新增/编辑表单的原始输入。解析与校验在本层完成，GUI 不写业务判断。
struct TaskForm {
    std::string title;
    std::string course;
    std::string kind{"other"};         // coursework/selfstudy/activity/review/other
    std::string importance{"3"};
    std::string estimate{"0"};
    std::string deadline;              // YYYY-MM-DD 或 YYYY-MM-DD HH:MM
    std::string start;                 // 仅活动：固定开始时刻
    std::optional<TaskState> state;    // 新增时忽略；编辑时保留原状态
};

// 计划视图的一行；字符串已经格式化好，外壳只负责显示。
struct PlanRow {
    std::string time_text;  // "18:30 - 19:30"；本周视图带日期
    std::string title;
    std::string kind_label;
};

struct PlanView {
    std::vector<PlanRow> rows;
    std::vector<std::string> warnings;  // 已带任务编号与中文原因
    Minutes total_free{0};              // 计划窗口总时长
    Minutes total_assigned{0};          // 已排入时间块的总时长
};

// 周统计的一行：类型、投入分钟、占比（向下取整）。
struct KindMinutesRow {
    std::string kind_label;
    Minutes minutes{0};
    int percent{0};
};

struct WeeklyStatsView {
    std::string window_text;  // "2026-10-19 ~ 2026-10-25"
    std::size_t completed{0};
    std::size_t expected{0};
    bool has_rate{false};
    int completion_percent{0};
    Minutes total_minutes{0};
    std::vector<KindMinutesRow> by_kind;
};

class AppModel {
public:
    AppModel(ITaskRepository& repo, std::function<TimePoint()> clock,
             ScheduleConfig config = ScheduleConfig{}, IEventLog* log = nullptr);

    Result<std::vector<TaskRow>> list(const ListQuery& query) const;
    Result<Stats> stats() const;

    // 编辑对话框的初始值。
    Result<TaskForm> form_for(TaskId id) const;

    Result<TaskId> create(const TaskForm& form);
    Status update(TaskId id, const TaskForm& form);
    Status remove(TaskId id);
    Status set_state(TaskId id, TaskState state);

    Result<PlanView> plan_today(RankMode rank = RankMode::Score) const;
    Result<PlanView> plan_week(RankMode rank = RankMode::Score) const;

    // M3：周统计与 SM-2 复习（ADR-0004）。
    Result<WeeklyStatsView> weekly_stats() const;
    Result<TaskRow> grade_review(TaskId id, int quality,
                                 std::optional<Minutes> spent = std::nullopt);
    Result<std::vector<TaskRow>> due_reviews() const;

private:
    Result<Task> build_task(TaskId id, const TaskForm& form) const;
    Result<PlanView> plan(bool week, RankMode rank) const;

    ITaskRepository& repo_;
    std::function<TimePoint()> clock_;
    ScheduleConfig config_;
    IEventLog* log_{nullptr};
};

}  // namespace ordo
