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

constexpr int kMinImportance = 1;
constexpr int kMaxImportance = 5;
constexpr int kDefaultImportance = 3;

struct Task {
    TaskId id{};
    std::string title;
    std::string course;
    int importance{kDefaultImportance};
    Minutes estimate{0};
    TimePoint deadline{};
    TaskState state{TaskState::Todo};
    int review_stage{0};
    std::optional<TimePoint> last_review;  // 仅 M3 使用，CSV 不持久化

    // 不变量校验：失败返回错误码，不做部分修复。
    Status validate() const;
};

// 与 CSV / CLI 共用的枚举文本形式：todo / doing / done / dropped。
std::string_view to_string(TaskState state) noexcept;
std::optional<TaskState> parse_task_state(std::string_view text) noexcept;

// 是否仍需投入时间（todo / doing）。
bool is_open(TaskState state) noexcept;

// 排序与统计用：把任务列表按状态与截止时间做确定性整理。
std::vector<Task> filter_by_state(const std::vector<Task>& tasks,
                                  std::optional<TaskState> state);

}  // namespace ordo
