#pragma once

// Task：系统里唯一的核心实体（OUTLINE §6）。
// 只存事实（截止时间、预估、重要度），不存派生量（紧急度、分数）——
// 派生量随"现在几点"变化，存下来必然过期。

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ordo/core/result.h"
#include "ordo/core/types.h"

namespace ordo {

enum class TaskState { Todo, Doing, Done, Dropped };

// 任务类型决定打分系数与装箱方式（ADR-0003）。
// Other 是旧数据的默认值，行为等同 SelfStudy。
enum class TaskKind { CourseWork, SelfStudy, Activity, Review, Other };

constexpr int kMinImportance = 1;
constexpr int kMaxImportance = 5;
constexpr int kDefaultImportance = 3;

// SM-2 参数的边界（ADR-0004）。上限 365 天是工程约束，不是 SM-2 原文。
constexpr double kMinEase = 1.3;
constexpr double kMaxEase = 5.0;
constexpr double kDefaultEase = 2.5;
constexpr int kMaxIntervalDays = 365;

struct Task {
    TaskId id{};
    std::string title;
    std::string course;
    int importance{kDefaultImportance};
    Minutes estimate{0};
    TimePoint deadline{};
    TaskState state{TaskState::Todo};
    int review_stage{0};                  // SM-2 重复次数，0 表示未进入复习
    double ease_factor{kDefaultEase};     // SM-2 难度系数
    int interval_days{0};                 // 当前复习间隔（天）
    std::optional<TimePoint> last_review;  // 上次复习时刻；为空表示从未复习
    TaskKind kind{TaskKind::Other};
    std::optional<TimePoint> fixed_start;  // 仅 Activity：固定开始时刻

    // 不变量校验：失败返回错误码，不做部分修复。
    Status validate() const;
};

// 与 CSV / CLI 共用的枚举文本形式：todo / doing / done / dropped。
std::string_view to_string(TaskState state) noexcept;
std::optional<TaskState> parse_task_state(std::string_view text) noexcept;

// 与 CSV / CLI 共用的类型文本形式：coursework / selfstudy / activity / review / other。
std::string_view to_string(TaskKind kind) noexcept;
std::optional<TaskKind> parse_task_kind(std::string_view text) noexcept;

// 面向界面的中文类型名：课程作业 / 自主学习 / 活动 / 复习考试 / 其他。
std::string_view kind_label(TaskKind kind) noexcept;

// 活动类任务带固定开始时间，装订时整块占位、不可拆分。
bool is_fixed_time(TaskKind kind) noexcept;

// 是否仍需投入时间（todo / doing）。
bool is_open(TaskState state) noexcept;

// 排序与统计用：把任务列表按状态与截止时间做确定性整理。
std::vector<Task> filter_by_state(const std::vector<Task>& tasks,
                                  std::optional<TaskState> state);

}  // namespace ordo
